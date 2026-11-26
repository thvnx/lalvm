// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2024-2026 The LALVM Project

#include "MLIRGenImpl.h"
#include "mlir/Dialect/ControlFlow/IR/ControlFlowOps.h"
#include "mlir/IR/Diagnostics.h"
#include "mlir/IR/MLIRContext.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallPtrSet.h"

namespace libadalang = frontend::libadalang;

namespace lalvm::mlirgen {

void MLIRGenImpl::anchorPendingLabels() {
  mlir::MLIRContext *ctx = builder.getContext();
  for (auto &[blk, name, labelLoc] : pendingLabels) {
    // Find an anchor for the label's low_pc: skip null operations and
    // promotable reads and writes, then chase a branch block to its target.
    // Consecutive labels may share an anchor, each with its own marker.
    mlir::Operation *anchor = nullptr;
    mlir::Block *b = blk;
    llvm::SmallPtrSet<mlir::Block *, 4> seen;
    while (b && seen.insert(b).second) {
      mlir::Operation *first = nullptr;
      for (mlir::Operation &o : *b)
        if (!mlir::isa<mlir::ada::NullOp, mlir::ada::ReadOp,
                       mlir::ada::WriteOp>(o)) {
          first = &o;
          break;
        }
      if (auto br = mlir::dyn_cast_or_null<mlir::cf::BranchOp>(first)) {
        b = br.getDest();
        continue;
      }
      anchor = first;
      break;
    }
    if (!anchor)
      continue;
    anchor->setLoc(mlir::FusedLoc::get(
        ctx, {anchor->getLoc()},
        mlir::ada::DILabelRefAttr::get(ctx, name, labelLoc)));
  }
}

mlir::LogicalResult MLIRGenImpl::mlirGenBlock(ada_node &blockNode,
                                              llvm::StringRef name) {
  bool isDecl = ada_node_kind(&blockNode) == ada_decl_block;

  auto scopeGuard = pushScope(makeUniqueInScope(name.empty() ? "b" : name));

  // Codegen the declarative part (ada_decl_block only).
  if (isDecl) {
    ada_node decls;
    ada_decl_block_f_decls(&blockNode, &decls);
    if (!ada_node_is_null(&decls))
      if (mlir::failed(mlirGenDeclarativePart(decls)))
        return mlir::failure();
  }

  // Codegen the statement sequence inline into the enclosing region.
  ada_node stmts;
  if (isDecl)
    ada_decl_block_f_stmts(&blockNode, &stmts);
  else
    ada_begin_block_f_stmts(&blockNode, &stmts);
  return visit(stmts);
}

mlir::Value MLIRGenImpl::mlirGenCondition(ada_node &cond) {
  mlir::Value c = visit_expr(cond);
  return c ? unwrap(c, loc(cond)) : c;
}

bool MLIRGenImpl::currentBlockTerminated() {
  mlir::Block *blk = builder.getInsertionBlock();
  return blk && !blk->empty() &&
         blk->back().hasTrait<mlir::OpTrait::IsTerminator>();
}

void MLIRGenImpl::branchToMergeIfOpen(mlir::Block *mergeBlock,
                                      mlir::Location location) {
  if (!currentBlockTerminated())
    mlir::cf::BranchOp::create(builder, location, mergeBlock);
}

void MLIRGenImpl::continueAtMerge(mlir::Block *merge, mlir::Block *fallback) {
  if (merge->hasNoPredecessors()) {
    merge->erase();
    builder.setInsertionPointToEnd(fallback);
  } else {
    builder.setInsertionPointToEnd(merge);
  }
}

mlir::Block *MLIRGenImpl::getOrCreateLabelBlock(ada_node defName) {
  ada_base_node key = canonicalDefName(defName).node;
  mlir::Block *&blk = labelBlocks[key];
  if (!blk) {
    blk = new mlir::Block();
    builder.getInsertionBlock()->getParent()->push_back(blk);
  }
  return blk;
}

mlir::LogicalResult MLIRGenImpl::mlirGenGoto(ada_node &goto_stmt) {
  ada_node nameNode, defName;
  ada_goto_stmt_f_label_name(&goto_stmt, &nameNode);
  if (!ada_name_p_referenced_defining_name(&nameNode, /*imprecise_fallback=*/0,
                                           &defName) ||
      ada_node_is_null(&defName))
    return mlir::emitError(loc(goto_stmt), "unresolved goto label");
  // A label in an enclosing body name-resolves from a nested subprogram
  // (implicit declaration, @rm{5-1}), but @rm{5-8} forbids the transfer;
  // reject it here or a dangling label block fails verification.
  ada_node gotoBody = libadalang::enclosingSubpBody(&goto_stmt);
  ada_node labelBody = libadalang::enclosingSubpBody(&defName);
  if (ada_node_is_null(&gotoBody) || ada_node_is_null(&labelBody) ||
      !ada_node_is_equivalent(&gotoBody, &labelBody))
    return mlir::emitError(loc(goto_stmt),
                           "goto label outside the current subprogram");
  mlir::cf::BranchOp::create(builder, loc(goto_stmt),
                             getOrCreateLabelBlock(defName));
  return mlir::success();
}

mlir::LogicalResult MLIRGenImpl::mlirGenLabel(ada_node &label) {
  ada_node decl, nameNode;
  ada_label_f_decl(&label, &decl);
  ada_label_decl_f_name(&decl, &nameNode);
  mlir::Block *blk = getOrCreateLabelBlock(nameNode);
  branchToMergeIfOpen(blk, loc(label));
  builder.setInsertionPointToEnd(blk);
  // A DWARF label needs no op of its own: record (block, name, loc) now and,
  // after the body, fuse a `DILabelRef` marker onto the block's next
  // lowering-surviving op (the label's low_pc anchor).
  pendingLabels.push_back(
      {blk, builder.getStringAttr(libadalang::getName(&nameNode)), loc(label)});
  return mlir::success();
}

mlir::LogicalResult MLIRGenImpl::mlirGenIf(ada_node &if_stmt) {
  auto guards = collectIfGuards(if_stmt);
  if (!guards)
    return mlir::failure();

  ada_node elsePart;
  ada_if_stmt_f_else_part(&if_stmt, &elsePart);
  bool hasElse = !ada_node_is_null(&elsePart);

  // Split off the merge/continuation block (empty: we are at block end).
  mlir::Block *condBlock = builder.getInsertionBlock();
  mlir::Block *mergeBlock = condBlock->splitBlock(builder.getInsertionPoint());

  mlir::Block *curTest = condBlock;
  for (auto [i, guard] : llvm::enumerate(*guards)) {
    bool last = (i + 1 == guards->size());

    builder.setInsertionPointToEnd(curTest);
    mlir::Value condI1 = mlirGenCondition(guard.condition);
    if (!condI1)
      return mlir::failure();

    // The then-block and the block taken when this condition is false.
    mlir::Block *thenBlock = builder.createBlock(mergeBlock);
    mlir::Block *falseBlock;
    if (!last || hasElse)
      falseBlock = builder.createBlock(mergeBlock); // next elsif test, or else
    else
      falseBlock = mergeBlock;

    builder.setInsertionPointToEnd(curTest);
    mlir::cf::CondBranchOp::create(builder, loc(guard.condition), condI1,
                                   thenBlock, falseBlock);

    // Fill the then-block; fall through to merge if it did not terminate.
    builder.setInsertionPointToEnd(thenBlock);
    if (mlir::failed(visit(guard.thenPart)))
      return mlir::failure();
    branchToMergeIfOpen(mergeBlock, loc(if_stmt));

    curTest = falseBlock; // the next elsif test, or the else block
  }

  // Else part: after the loop curTest is the else block (when hasElse).
  if (hasElse) {
    builder.setInsertionPointToEnd(curTest);
    ada_node elseStmts;
    ada_else_part_f_stmts(&elsePart, &elseStmts);
    if (mlir::failed(visit(elseStmts)))
      return mlir::failure();
    branchToMergeIfOpen(mergeBlock, loc(if_stmt));
  }

  continueAtMerge(mergeBlock, condBlock);
  return mlir::success();
}

mlir::LogicalResult MLIRGenImpl::mlirGenLoop(ada_node &loopNode,
                                             llvm::StringRef name) {
  ada_node spec;
  ada_base_loop_stmt_f_spec(&loopNode, &spec);
  if (!ada_node_is_null(&spec) && ada_node_kind(&spec) == ada_for_loop_spec)
    return mlirGenForLoop(loopNode, spec, name);
  bool isWhile =
      !ada_node_is_null(&spec) && ada_node_kind(&spec) == ada_while_loop_spec;
  if (!ada_node_is_null(&spec) && !isWhile) {
    mlir::emitError(loc(loopNode), "unsupported loop specification");
    return mlir::failure();
  }

  ada_node body;
  ada_base_loop_stmt_f_stmts(&loopNode, &body);

  // Split off the (empty) merge block, then insert the loop blocks before it:
  // entry -> [header ->] body -> merge.
  mlir::Block *entryBlock = builder.getInsertionBlock();
  mlir::Block *mergeBlock = entryBlock->splitBlock(builder.getInsertionPoint());
  mlir::Block *bodyBlock = builder.createBlock(mergeBlock);

  // `while` tests the condition in a header each iteration; a bare loop has
  // none and re-enters the body directly.
  mlir::Block *headerBlock = bodyBlock;
  if (isWhile) {
    headerBlock = builder.createBlock(bodyBlock);
    ada_node cond;
    ada_while_loop_spec_f_expr(&spec, &cond);
    builder.setInsertionPointToEnd(headerBlock);
    mlir::Value condI1 = mlirGenCondition(cond);
    if (!condI1)
      return mlir::failure();
    mlir::cf::CondBranchOp::create(builder, loc(cond), condI1, bodyBlock,
                                   mergeBlock);
  }

  builder.setInsertionPointToEnd(entryBlock);
  mlir::cf::BranchOp::create(builder, loc(loopNode), headerBlock);

  // Body, with the loop on `loopStack` for `exit`; close with the back-edge
  // to the header unless the body already terminated.
  builder.setInsertionPointToEnd(bodyBlock);
  loopStack.push_back({mergeBlock, name.str()});
  auto loopGuard = llvm::scope_exit([&] { loopStack.pop_back(); });
  if (mlir::failed(visit(body)))
    return mlir::failure();
  branchToMergeIfOpen(headerBlock, loc(loopNode));

  continueAtMerge(mergeBlock, bodyBlock);
  return mlir::success();
}

mlir::LogicalResult MLIRGenImpl::mlirGenForLoop(ada_node &loopNode,
                                                ada_node &spec,
                                                llvm::StringRef name) {
  // `for ... of` container iteration is deferred.
  ada_node loopType;
  ada_for_loop_spec_f_loop_type(&spec, &loopType);
  if (ada_node_kind(&loopType) != ada_iter_type_in) {
    mlir::emitError(loc(loopNode), "`for ... of` loops are not yet supported");
    return mlir::failure();
  }

  // Iterator filters (`when`) are deferred.
  ada_node filter;
  ada_for_loop_spec_f_iter_filter(&spec, &filter);
  if (!ada_node_is_null(&filter)) {
    mlir::emitError(loc(loopNode),
                    "`for` loop iterator filters are not yet supported");
    return mlir::failure();
  }

  // Only an explicit `L .. H` range is supported; a range given by a type or
  // subtype name or a Range attribute (needing First and Last) is diagnosed.
  ada_node iterExpr;
  ada_for_loop_spec_f_iter_expr(&spec, &iterExpr);
  auto bounds = libadalang::explicitRangeBounds(iterExpr);
  if (!bounds) {
    mlir::emitError(
        loc(loopNode),
        "`for` loop over a type or subtype name is not yet supported");
    return mlir::failure();
  }

  ada_node reverseNode;
  ada_for_loop_spec_f_has_reverse(&spec, &reverseNode);
  bool isReverse = ada_node_kind(&reverseNode) == ada_reverse_present;

  // Loop parameter and its subtype (@rm{5-5}(6)): from an explicit type mark
  // when present, else the range's type.
  ada_node varDecl;
  ada_for_loop_spec_f_var_decl(&spec, &varDecl);
  ada_node id;
  ada_for_loop_var_decl_f_id(&varDecl, &id);
  ada_node idType;
  ada_for_loop_var_decl_f_id_type(&varDecl, &idType);
  ada_node typeDecl{};
  if (!ada_node_is_null(&idType))
    typeDecl = libadalang::designatedTypeDecl(idType).value_or(ada_node{});
  else
    typeDecl = libadalang::expressionType(iterExpr).value_or(ada_node{});
  if (ada_node_is_null(&typeDecl)) {
    mlir::emitError(loc(loopNode),
                    "failed to resolve `for` loop parameter type");
    return mlir::failure();
  }
  mlir::ada::QualType paramType = getAdaQualType(typeDecl, loc(loopNode));
  if (!paramType)
    return mlir::failure();

  // Reject non-integer ranges: an enumeration or real range would step and
  // compare on the representation (wrong for a non-contiguous enumeration).
  // Integer and modular types carry an `IntegerTypeInfoAttr`; others do not.
  if (mlir::ada::TypeOp typeOp = lookupOrEmitTypeOp(typeDecl, loc(loopNode)))
    if (!mlir::isa_and_nonnull<mlir::ada::IntegerTypeInfoAttr>(
            typeOp.getTypeInfoAttr())) {
      mlir::emitError(loc(loopNode),
                      "`for` loop over a non-integer range is not yet "
                      "supported");
      return mlir::failure();
    }

  // Boolean result type for the range guard and the pre-step bound test.
  ada_node boolDecl = resolveBooleanTypeDecl(loopNode, loc(loopNode));
  if (ada_node_is_null(&boolDecl))
    return mlir::failure();
  mlir::ada::QualType boolType = getAdaQualType(boolDecl, loc(loopNode));
  if (!boolType)
    return mlir::failure();

  // Compare two induction values, unwrapped to the `i1` a `cf` branch needs.
  auto cmpI1 = [&](mlir::ada::AdaRelationalOp kind, mlir::Value a,
                   mlir::Value b) -> mlir::Value {
    return unwrap(
        mlir::ada::CmpOp::create(builder, loc(loopNode), boolType, kind, a, b),
        loc(loopNode));
  };

  // Blocks in source order: entry -> init -> body -> step -> merge.
  mlir::Block *entryBlock = builder.getInsertionBlock();
  mlir::Block *mergeBlock = entryBlock->splitBlock(builder.getInsertionPoint());
  mlir::Block *stepBlock = builder.createBlock(mergeBlock);
  mlir::Block *bodyBlock = builder.createBlock(stepBlock);
  mlir::Block *initBlock = builder.createBlock(bodyBlock);

  // Evaluate bounds once and guard the empty range.
  builder.setInsertionPointToEnd(entryBlock);
  auto [lowNode, highNode] = *bounds;

  // A statically null range (`lo > hi`) provably runs zero iterations; warn.
  // @todo Libadalang 26 evaluates enumeration 'Pos using representation
  // codes, so this warning can report a nonempty range as null.
  if (libadalang::isStaticExpr(lowNode) && libadalang::isStaticExpr(highNode))
    if (auto staticLo = libadalang::evalExprAsInt(lowNode))
      if (auto staticHi = libadalang::evalExprAsInt(highNode))
        if (llvm::APSInt::compareValues(*staticLo, *staticHi) > 0)
          mlir::emitWarning(loc(loopNode),
                            "loop range is null, loop will not execute");

  mlir::Value lo = visit_expr(lowNode);
  if (!lo)
    return mlir::failure();
  mlir::Value hi = visit_expr(highNode);
  if (!hi)
    return mlir::failure();
  // Convert each bound to the loop parameter's subtype.
  lo = convertImplicitly(lo, paramType, loc(lowNode));
  if (!lo)
    return mlir::failure();
  hi = convertImplicitly(hi, paramType, loc(highNode));
  if (!hi)
    return mlir::failure();

  auto nameAttr = getNameAttr(id);
  auto paramVal = bodyBlock->addArgument(paramType, loc(id));
  setAdaNameLoc(mlir::Value(paramVal), nameAttr);
  declare(id, paramVal);

  mlir::Value guardI1 = cmpI1(mlir::ada::AdaRelationalOp::Lte, lo, hi);
  if (!guardI1)
    return mlir::failure();
  mlir::cf::CondBranchOp::create(builder, loc(loopNode), guardI1, initBlock,
                                 mergeBlock);

  // Enter with the first iteration's value.
  builder.setInsertionPointToEnd(initBlock);
  mlir::cf::BranchOp::create(builder, loc(loopNode), bodyBlock,
                             mlir::ValueRange{isReverse ? hi : lo});

  // Body, with the loop on `loopStack` for `exit`. Its fall-through runs the
  // pre-step bound test: exit at the far bound, else step.
  builder.setInsertionPointToEnd(bodyBlock);
  ada_node body;
  ada_base_loop_stmt_f_stmts(&loopNode, &body);
  loopStack.push_back({mergeBlock, name.str()});
  auto loopGuard = llvm::scope_exit([&] { loopStack.pop_back(); });
  if (mlir::failed(visit(body)))
    return mlir::failure();

  if (!currentBlockTerminated()) {
    mlir::Value atEndI1 =
        cmpI1(mlir::ada::AdaRelationalOp::Eq, paramVal, isReverse ? lo : hi);
    if (!atEndI1)
      return mlir::failure();
    mlir::cf::CondBranchOp::create(builder, loc(loopNode), atEndI1, mergeBlock,
                                   stepBlock);

    // The bound test makes the step safe without an overflow check.
    builder.setInsertionPointToEnd(stepBlock);
    auto intType = mlir::cast<mlir::IntegerType>(paramType.getMlirType());
    mlir::Value one = emitIntConstant(llvm::APInt(intType.getWidth(), 1),
                                      paramType, loc(loopNode));
    mlir::Value next =
        mlir::ada::BinOp::create(builder, loc(loopNode), paramType,
                                 isReverse ? mlir::ada::AdaBinaryOp::Minus
                                           : mlir::ada::AdaBinaryOp::Plus,
                                 paramVal, one, mlir::ada::AdaChecksAttr{});
    mlir::cf::BranchOp::create(builder, loc(loopNode), bodyBlock,
                               mlir::ValueRange{next});
  } else {
    // The body never falls through (e.g. it always returns): no step or
    // back-edge is reachable.
    stepBlock->erase();
  }

  builder.setInsertionPointToEnd(mergeBlock);
  return mlir::success();
}

mlir::LogicalResult MLIRGenImpl::mlirGenExit(ada_node &exit_stmt) {
  if (loopStack.empty()) {
    mlir::emitError(loc(exit_stmt), "exit outside of a loop");
    return mlir::failure();
  }

  mlir::Block *target = nullptr;
  ada_node nameNode;
  ada_exit_stmt_f_loop_name(&exit_stmt, &nameNode);
  if (ada_node_is_null(&nameNode)) {
    target = loopStack.back().first; // innermost
  } else {
    std::string name = libadalang::getName(&nameNode);
    for (auto &entry : llvm::reverse(loopStack))
      if (llvm::StringRef(entry.second).equals_insensitive(name)) {
        target = entry.first;
        break;
      }
    if (!target) {
      mlir::emitError(loc(exit_stmt), "no enclosing loop named '")
          << name << "'";
      return mlir::failure();
    }
  }

  ada_node cond;
  ada_exit_stmt_f_cond_expr(&exit_stmt, &cond);
  if (ada_node_is_null(&cond)) {
    mlir::cf::BranchOp::create(builder, loc(exit_stmt), target);
    return mlir::success();
  }

  // `exit when C`: continue the body in a fresh block taken when C is false.
  mlir::Block *current = builder.getInsertionBlock();
  mlir::Block *contBlock = current->splitBlock(builder.getInsertionPoint());
  builder.setInsertionPointToEnd(current);
  mlir::Value condI1 = mlirGenCondition(cond);
  if (!condI1)
    return mlir::failure();
  mlir::cf::CondBranchOp::create(builder, loc(exit_stmt), condI1, target,
                                 contBlock);
  builder.setInsertionPointToEnd(contBlock);
  return mlir::success();
}

mlir::LogicalResult MLIRGenImpl::mlirGenCallStmt(ada_node &call_stmt) {
  ada_node call;
  ada_call_stmt_f_call(&call_stmt, &call);
  return mlirGenCallExpr(call) ? mlir::success() : mlir::failure();
}

mlir::LogicalResult MLIRGenImpl::mlirGenAssign(ada_node &assign_stmt) {
  ada_node dest_node, expr_node;
  ada_assign_stmt_f_dest(&assign_stmt, &dest_node);
  ada_assign_stmt_f_expr(&assign_stmt, &expr_node);

  mlir::Value rhs = visit_expr(expr_node);
  if (!rhs)
    return mlir::failure();

  ada_node_kind_enum kind = ada_node_kind(&dest_node);
  mlir::Value ptr;

  switch (kind) {
  case ada_identifier: {
    ptr = findVarValue(dest_node);
    if (!ptr) {
      mlir::emitError(loc(dest_node), "unknown variable '")
          << libadalang::getName(&dest_node, false) << "'";
      return mlir::failure();
    }
    break;
  }
  case ada_call_expr: {
    ada_call_expr_kind call_kind = {};
    if (ada_call_expr_p_kind(&dest_node, &call_kind) &&
        call_kind == ADA_CALL_EXPR_KIND_ARRAY_INDEX) {
      ptr = indexedElementRef(dest_node);
      if (!ptr)
        return mlir::failure(); // error already emitted
      break;
    }
  }
    [[fallthrough]];
  default: {
    mlir::emitError(loc(assign_stmt), "unsupported assignment destination");
    return mlir::failure();
  }
  }

  // A loop parameter is a constant view (@rm{5-5}(6)).
  std::optional<ada_node> ref_decl = libadalang::referencedDecl(dest_node);
  if (ref_decl && ada_node_kind(&ref_decl.value()) == ada_for_loop_var_decl) {
    mlir::emitError(loc(dest_node), "assignment to loop parameter not allowed");
    return mlir::failure();
  }

  auto refType = mlir::dyn_cast<mlir::ada::RefType>(ptr.getType());
  if (!refType) {
    mlir::emitError(loc(dest_node), "cannot assign to '")
        << libadalang::getName(&dest_node, false) << "'";
    return mlir::failure();
  }

  // Convert to the subtype of the target (@rm{5-2}(11)).
  auto targetType = mlir::cast<mlir::ada::QualType>(refType.getValueType());
  rhs = convertImplicitly(rhs, targetType, loc(assign_stmt));
  if (!rhs)
    return mlir::failure();

  auto location = propagateAdaNameLoc(ptr, loc(dest_node));
  mlir::ada::WriteOp::create(builder, location, rhs, ptr);
  return mlir::success();
}

mlir::FailureOr<mlir::Value>
MLIRGenImpl::mlirGenReturnExpr(ada_node &node, mlir::ada::SubpOp subp) {
  auto returnLoc = loc(node);
  ada_node return_expr = node;
  if (ada_node_kind(&node) == ada_return_stmt)
    ada_return_stmt_f_return_expr(&node, &return_expr);
  // A simple return has an expression iff it applies to a function body
  // (@rm{6-5}(5)). Libadalang does not check it.
  // @todo Check unreachable returns too, and exempt the simple returns of an
  //       extended return statement (never an expression) once supported.
  if (!subp)
    return mlir::emitError(returnLoc,
                           "return statement has no enclosing subprogram");
  bool hasExpr = !ada_node_is_null(&return_expr);
  if (subp.isFunction() && !hasExpr)
    return mlir::emitError(returnLoc,
                           "missing expression in return from function");
  if (subp.isProcedure() && hasExpr)
    return mlir::emitError(returnLoc,
                           "procedure cannot return value (use function)");

  if (!hasExpr)
    return mlir::Value{};
  mlir::Value expr = visit_expr(return_expr);
  if (!expr)
    return mlir::failure();
  // Convert to the subprogram's result subtype (@rm{6-5}).
  auto resultType =
      mlir::cast<mlir::ada::QualType>(subp.getFunctionType().getResult(0));
  expr = convertImplicitly(expr, resultType, loc(return_expr));
  if (!expr)
    return mlir::failure();
  return expr;
}

mlir::LogicalResult MLIRGenImpl::mlirGenReturn(ada_node &return_stmt) {
  auto subp = builder.getInsertionBlock()
                  ->getParent()
                  ->getParentOfType<mlir::ada::SubpOp>();
  mlir::FailureOr<mlir::Value> expr = mlirGenReturnExpr(return_stmt, subp);

  if (mlir::failed(expr))
    return mlir::failure();

  mlir::ada::ReturnOp::create(builder, loc(return_stmt), expr.value());
  return mlir::success();
}

} // namespace lalvm::mlirgen
