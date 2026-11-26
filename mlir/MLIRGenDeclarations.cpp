// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2024-2026 The LALVM Project

#include "MLIRGenImpl.h"
#include "mlir/IR/Diagnostics.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/SaveAndRestore.h"

namespace libadalang = frontend::libadalang;

namespace lalvm::mlirgen {

mlir::ada::CallOp MLIRGenImpl::mlirGenCallExpr(ada_node &call) {
  auto location = loc(call);

  ada_node name_node;
  ada_node suffix{};

  switch (ada_node_kind(&call)) {
  case ada_identifier:
    name_node = call;
    break;
  case ada_call_expr:
    ada_call_expr_f_name(&call, &name_node);
    ada_call_expr_f_suffix(&call, &suffix);
    break;
  default:
    mlir::emitError(location, "unsupported call expression");
    return nullptr;
  }

  auto calleeName = libadalang::getName(&name_node);

  ada_node defName;
  if (!ada_name_p_referenced_defining_name(&name_node,
                                           /*imprecise_fallback=*/false,
                                           &defName) ||
      ada_node_is_null(&defName)) {
    mlir::emitError(location, "unknown subprogram '") << calleeName << "'";
    return nullptr;
  }
  auto calleeSubp = subpDecls.lookup(canonicalDefName(defName).node);
  if (!calleeSubp) {
    mlir::emitError(location, "unknown subprogram '") << calleeName << "'";
    return nullptr;
  }

  auto funcTy = calleeSubp.getFunctionType();
  unsigned count = funcTy.getNumInputs();
  llvm::SmallVector<mlir::Value> args(count), snapshots(count), ranges(count);
  llvm::SmallVector<int32_t> order;
  if (!ada_node_is_null(&suffix)) {
    ada_internal_param_actual_array bindings = nullptr;
    if (!ada_assoc_list_p_zip_with_params(&suffix, false, &bindings) ||
        !bindings) {
      mlir::emitError(location, "failed to bind call associations");
      return nullptr;
    }
    auto bindingsGuard = llvm::scope_exit(
        [&] { ada_internal_param_actual_array_dec_ref(bindings); });
    llvm::StringMap<unsigned> formalIndices;
    for (auto [i, formal] : llvm::enumerate(subpFormals[calleeSubp]))
      formalIndices[libadalang::getName(&formal)] = i;
    llvm::DenseMap<ada_base_node, unsigned> actualFormals;
    for (int b = 0; b < bindings->n; ++b) {
      auto &binding = bindings->items[b];
      auto formal = formalIndices.find(libadalang::getName(&binding.param));
      if (formal != formalIndices.end())
        actualFormals[binding.actual.node] = formal->second;
    }
    for (unsigned n = 0; n < ada_node_children_count(&suffix); ++n) {
      ada_node assoc, expr;
      ada_node_child(&suffix, n, &assoc);
      ada_param_assoc_f_r_expr(&assoc, &expr);
      auto formalIndex = actualFormals.find(expr.node);
      if (formalIndex == actualFormals.end() || args[formalIndex->second]) {
        mlir::emitError(loc(assoc),
                        "failed to resolve formal for call association");
        return nullptr;
      }
      unsigned i = formalIndex->second;
      order.push_back(i);
      mlir::Type formal = funcTy.getInput(i);
      auto ref = mlir::dyn_cast<mlir::ada::RefType>(formal);
      if (!ref) {
        mlir::Value value = visit_expr(expr);
        if (!value)
          return nullptr;
        args[i] = convertImplicitly(
            value, mlir::cast<mlir::ada::QualType>(formal), loc(expr));
        if (!args[i])
          return nullptr;
        continue;
      }
      args[i] = resolveVarPtr(expr);
      if (!args[i])
        return nullptr;
      bool inOut =
          calleeSubp.getFormalMode(i) == mlir::ada::AdaParamMode::InOut;
      if (calleeSubp.formalNeedsInput(i)) {
        mlir::Value value = readObject(args[i], loc(expr), !inOut);
        if (!value)
          return nullptr;
        auto nominal = mlir::cast<mlir::ada::QualType>(ref.getValueType());
        if (inOut)
          value = convertImplicitly(value, nominal, loc(expr));
        else {
          auto base = mlir::ada::baseOfAdaType(calleeSubp, nominal);
          if (!base) {
            mlir::emitError(loc(expr), "unresolved formal base type");
            return nullptr;
          }
          value = mlir::ada::requalify(builder, loc(expr), value, base);
        }
        if (!value)
          return nullptr;
        snapshots[i] = value;
      }
      auto actual = mlir::ada::objectValueType(args[i].getType());
      auto actualType = mlir::ada::lookupAdaType(
          builder.getInsertionBlock()->getParentOp(), actual.getAdaType());
      if (actualType && actualType.hasDynamicRange()) {
        ranges[i] = findDynamicRange(actual.getAdaType());
        if (!ranges[i]) {
          mlir::emitError(loc(expr), "missing dynamic actual subtype range");
          return nullptr;
        }
      }
    }
  }
  if (order.size() != count) {
    mlir::emitError(location, "omitted default arguments are not supported");
    return nullptr;
  }
  llvm::SmallVector<mlir::Value> initialValues, copybackRanges;
  llvm::SmallVector<int32_t> initialFormals, rangeFormals;
  for (unsigned i = 0; i < count; ++i) {
    if (snapshots[i]) {
      initialValues.push_back(snapshots[i]);
      initialFormals.push_back(i);
    }
    if (ranges[i]) {
      copybackRanges.push_back(ranges[i]);
      rangeFormals.push_back(i);
    }
  }
  auto calleeRef = mlir::FlatSymbolRefAttr::get(builder.getContext(),
                                                resolveSymbol(defName));

  mlir::Type resultType =
      calleeSubp.isFunction() ? funcTy.getResult(0) : mlir::Type{};
  auto result =
      mlir::ada::CallOp::create(builder, location, calleeRef, resultType, args,
                                initialValues, copybackRanges);
  if (!llvm::is_sorted(order))
    result->setAttr("ada.actual_order", builder.getDenseI32ArrayAttr(order));
  if (!initialFormals.empty())
    result->setAttr("ada.initial_formals",
                    builder.getDenseI32ArrayAttr(initialFormals));
  if (!rangeFormals.empty())
    result->setAttr("ada.range_formals",
                    builder.getDenseI32ArrayAttr(rangeFormals));
  return result;
}

mlir::LogicalResult MLIRGenImpl::mlirGenNumberDecl(ada_node &number_decl) {
  ada_node expr;
  ada_number_decl_f_expr(&number_decl, &expr);

  // Determine the universal type of the static expression.
  auto exprType = libadalang::expressionType(expr);

  enum class UniversalKind { Unknown, Int, Real };
  UniversalKind kind = UniversalKind::Unknown;
  std::string unknownTypeName;
  if (exprType) {
    ada_bool integer = false;
    if (ada_base_type_decl_p_is_int_type(&exprType.value(),
                                         &libadalang::kNullOrigin, &integer) &&
        integer)
      kind = UniversalKind::Int;
    else if (libadalang::isUniversalTypeDecl(exprType.value()) &&
             libadalang::isRealTypeDecl(exprType.value()))
      kind = UniversalKind::Real;
    else if (auto name = libadalang::declDefiningName(exprType.value()))
      unknownTypeName = libadalang::getName(&name.value());
  }
  if (kind != UniversalKind::Unknown) {
    ada_node universal{};
    auto getType = kind == UniversalKind::Int
                       ? ada_ada_node_p_universal_int_type
                       : ada_ada_node_p_universal_real_type;
    if (!getType(&expr, &universal) || ada_node_is_null(&universal))
      return mlir::emitError(loc(number_decl),
                             "failed to resolve named number type");
    exprType = universal;
  }

  // Preserve the exact integer value independently of its debug constant.
  std::optional<llvm::APSInt> integer;
  mlir::TypedAttr constAttr;
  mlir::ada::TypeOp typeOp;
  switch (kind) {
  case UniversalKind::Int: {
    integer = libadalang::evalExprAsInt(expr);
    if (integer) {
      typeOp = lookupOrEmitTypeOp(exprType.value(), loc(number_decl));
      if (typeOp) {
        auto intType = mlir::cast<mlir::IntegerType>(typeOp.getMlirType());
        if (integer->getSignificantBits() <= intType.getWidth())
          constAttr = mlir::IntegerAttr::get(
              intType, integer->sextOrTrunc(intType.getWidth()));
      }
    }
    break;
  }
  case UniversalKind::Real: {
    // Parentheses only group syntactically (@rm{4-4}): strip them to reach
    // the literal node, as `visit_expr` does when emitting. This cannot
    // reuse `visit_expr` itself, which resolves a concrete type from the
    // use context; here the value must stay at universal type.
    ada_node literal = expr;
    while (ada_node_kind(&literal) == ada_paren_expr) {
      ada_node inner;
      if (ada_paren_expr_f_expr(&literal, &inner) == 0 ||
          ada_node_is_null(&inner))
        break;
      literal = inner;
    }
    if (ada_node_kind(&literal) == ada_real_literal) {
      auto value = evalRealLiteral(literal);
      if (value) {
        typeOp = lookupOrEmitTypeOp(exprType.value(), loc(number_decl));
        if (typeOp)
          constAttr = mlir::FloatAttr::get(
              mlir::cast<mlir::FloatType>(typeOp.getMlirType()), *value);
      }
    }
    break;
  }
  case UniversalKind::Unknown: {
    auto diag =
        mlir::emitError(loc(number_decl), "named number has unsupported type");
    if (!unknownTypeName.empty())
      diag << " '" << unknownTypeName << "'";
    return mlir::failure();
  }
  }

  ada_node ids;
  ada_number_decl_f_ids(&number_decl, &ids);
  unsigned count = ada_node_children_count(&ids);
  for (unsigned i = 0; i < count; ++i) {
    ada_node id;
    if (ada_node_child(&ids, i, &id) == 0) {
      mlir::emitError(loc(number_decl), "failed to get declared identifier");
      return mlir::failure();
    }
    mlir::Value constValue;
    if (constAttr && typeOp) {
      auto nameAttr = getNameAttr(id);
      auto typedType = qualTypeFor(typeOp, constAttr.getType());
      auto constOp =
          mlir::ada::ConstantOp::create(builder, loc(id), typedType, constAttr);
      setAdaNameLoc(mlir::Value(constOp), nameAttr);
      constValue = constOp.getResult();
    }
    numberDecls[id] = {integer, constValue};
  }
  return mlir::success();
}

mlir::LogicalResult MLIRGenImpl::mlirGenObjectDecl(ada_node &object_decl) {
  auto declLoc = loc(object_decl);

  ada_node type_expr;
  ada_object_decl_f_type_expr(&object_decl, &type_expr);
  std::optional<ada_node> typeDecl = libadalang::designatedTypeDecl(type_expr);
  if (!typeDecl)
    return mlir::emitError(loc(type_expr), "failed to resolve type expression");
  mlir::ada::QualType nominalType =
      getAdaQualType(typeDecl.value(), loc(type_expr));
  if (!nominalType)
    return mlir::failure();

  // A constraint in the subtype indication gives each object its own
  // anonymous nominal subtype (@rm{3-3-1}(7-8)).
  std::optional<std::pair<ada_node, ada_node>> bounds;
  if (std::optional<ada_node> constraint =
          libadalang::subtypeConstraint(type_expr)) {
    bounds = libadalang::rangeConstraintBounds(constraint.value());
    if (!bounds)
      return mlir::emitError(loc(constraint.value()),
                             "this form of constraint is not supported");
  }

  ada_bool isConstant = 0;
  ada_basic_decl_p_is_constant_object(&object_decl, &isConstant);

  ada_node default_expr;
  ada_object_decl_f_default_expr(&object_decl, &default_expr);

  // Emit an error if the initialization expression is an array object.
  if (mlir::isa<mlir::ada::ArrayType>(nominalType.getMlirType()) &&
      !ada_node_is_null(&default_expr))
    return mlir::emitError(loc(default_expr),
                           "array object initialization is not supported");

  ada_node ids;
  ada_object_decl_f_ids(&object_decl, &ids);
  unsigned count = ada_node_children_count(&ids);
  for (unsigned i = 0; i < count; ++i) {
    ada_node id;
    if (ada_node_child(&ids, i, &id) == 0) {
      mlir::emitError(declLoc, "failed to get declared identifier");
      return mlir::failure();
    }
    auto nameAttr = getNameAttr(id);

    auto elemType = nominalType;
    if (bounds) {
      mlir::ada::TypeOp sub = emitAnonymousSubtype(
          id, typeDecl.value(), bounds->first, bounds->second, loc(id));
      if (!sub)
        return mlir::failure();
      elemType = mlir::ada::QualType::get(
          builder.getContext(), sub.getMlirType(),
          mlir::FlatSymbolRefAttr::get(sub.getSymNameAttr()));
    }

    mlir::Value init;
    if (!ada_node_is_null(&default_expr)) {
      init = visit_expr(default_expr);
      if (!init)
        return mlir::failure();
    }

    if (isConstant) {
      if (!init) {
        mlir::emitError(loc(id),
                        "deferred constant declarations are not supported");
        return mlir::failure();
      }
      // Convert to the object's subtype (@rm{3-3-1}).
      init = convertImplicitly(init, elemType, loc(id));
      if (!init)
        return mlir::failure();
      // A constant view must not rename the parameter it aliases.
      if (!mlir::isa<mlir::BlockArgument>(init))
        setAdaNameLoc(init, nameAttr, loc(id));
      declare(id, init);
    } else {
      // No explicit initializer: default-initialize from the type's
      // Default_Value aspect (@rm{3-5}) if it has one. (A constant object
      // instead requires an initializer, handled above.)
      if (!init)
        if (auto value = libadalang::defaultValueExpr(typeDecl.value())) {
          init = visit_expr(value.value());
          if (!init)
            return mlir::failure();
        }
      if (init) {
        init = convertImplicitly(init, elemType, loc(id));
        if (!init)
          return mlir::failure();
      }
      mlir::Value ptr = mlir::ada::ObjectOp::create(
          builder, loc(id),
          mlir::ada::RefType::get(builder.getContext(), elemType), init);
      setAdaNameLoc(ptr, nameAttr);
      declare(id, ptr);
    }
  }
  return mlir::success();
}

mlir::LogicalResult MLIRGenImpl::mlirGenDeclarativePart(ada_node &decls) {
  // Visit every declaration in source order, invoking `fn`.
  auto forEachDecl = [&](llvm::function_ref<mlir::LogicalResult(ada_node &)> fn)
      -> mlir::LogicalResult {
    unsigned listCount = ada_node_children_count(&decls);
    for (unsigned i = 0; i < listCount; ++i) {
      ada_node list;
      if (ada_node_child(&decls, i, &list) == 0)
        return mlir::emitError(loc(decls), "failed to get declarative list");
      unsigned count = ada_node_children_count(&list);
      for (unsigned j = 0; j < count; ++j) {
        ada_node decl;
        if (ada_node_child(&list, j, &decl) == 0)
          return mlir::emitError(loc(decls), "failed to get declaration");
        if (mlir::failed(fn(decl)))
          return mlir::failure();
      }
    }
    return mlir::success();
  };

  bool atLibraryLevel = builder.getInsertionBlock() == adaModule.getBody();
  llvm::SaveAndRestore<mlir::Block *> savedDeclsBlock(currentDeclsBlock,
                                                      nullptr);

  // Emit one declaration at the current insertion point. Return failure for
  // kinds we don't handle.
  auto emitDecl = [&](ada_node &decl) -> mlir::LogicalResult {
    if (mlir::failed(checkResolution(decl)))
      return mlir::failure();
    switch (ada_node_kind(&decl)) {
    case ada_number_decl:
      return mlirGenNumberDecl(decl);
    case ada_object_decl:
      // Library-level objects are globals (not a stack slot). Requires more
      // Ada dialect support to handle globals!
      if (atLibraryLevel)
        return mlir::emitError(loc(decl),
                               "library-level objects are not supported");
      return mlirGenObjectDecl(decl);
    case ada_concrete_type_decl:
    case ada_subtype_decl:
      if (isSupportedTypeDecl(decl))
        return mlirGenTypeDecl(decl);
      return mlir::success();
    case ada_subp_body:
    case ada_null_subp_decl:
    case ada_expr_function:
      return mlirGenSubpBody(decl) ? mlir::success() : mlir::failure();
    // The following nodes are queried when needed, no need to visit them.
    case ada_subp_decl:
    case ada_enum_rep_clause:
      return mlir::success();
    default:
      mlir::emitError(loc(decl),
                      "unsupported declaration in declarative part: ")
          << libadalang::image(&decl);
      return mlir::failure();
    }
  };
  // ada.subp/ada.type are symbols routed into the interior ada.decls; locals
  // (objects, numbers) stay inline in the region.
  auto isSymbol = [&](ada_node &decl) {
    auto kind = ada_node_kind(&decl);
    return ((kind == ada_concrete_type_decl || kind == ada_subtype_decl) &&
            isSupportedTypeDecl(decl)) ||
           kind == ada_subp_body || kind == ada_null_subp_decl ||
           kind == ada_expr_function;
  };

  // An object with a constraint in its subtype indication stays inline, but
  // its anonymous subtype is a symbol (see emitAnonymousSubtype).
  auto hasAnonymousSubtype = [&](ada_node &decl) {
    ada_node typeExpr = {};
    return ada_node_kind(&decl) == ada_object_decl &&
           ada_object_decl_f_type_expr(&decl, &typeExpr) &&
           libadalang::subtypeConstraint(typeExpr).has_value();
  };

  bool hasSymbols = false;
  if (mlir::failed(forEachDecl([&](ada_node &decl) {
        hasSymbols |= isSymbol(decl) || hasAnonymousSubtype(decl);
        return mlir::success();
      })))
    return mlir::failure();

  // No symbols or library-level declarative part: emit the locals inline
  // in source order, no ada.decls needed.
  if (!hasSymbols || atLibraryLevel)
    return forEachDecl(emitDecl);

  // Otherwise route each declaration in source order: locals just before the
  // ada.decls, symbols into it. mlirGenSubpBody moves the insertion point
  // into the nested subprogram, but the next iteration resets it here.
  auto declsOp = mlir::ada::DeclsOp::create(builder, loc(decls));
  mlir::Block *declsBlock = builder.createBlock(&declsOp.getBody());
  currentDeclsBlock = declsBlock;
  if (mlir::failed(forEachDecl([&](ada_node &decl) {
        if (isSymbol(decl))
          builder.setInsertionPointToEnd(declsBlock);
        else
          builder.setInsertionPoint(declsOp);
        return emitDecl(decl);
      })))
    return mlir::failure();
  builder.setInsertionPointAfter(declsOp);
  return mlir::success();
}

mlir::Operation *MLIRGenImpl::mlirGenSubpBody(ada_node &subp_body) {
  // Restore the insertion point after the new op on exit.
  mlir::OpBuilder::InsertionGuard guard(builder);

  ada_node ada_subp_spec;
  ada_base_subp_body_f_subp_spec(&subp_body, &ada_subp_spec);

  auto resultType = libadalang::subpResultType(ada_subp_spec);
  if (resultType) {
    auto resultDecl = libadalang::designatedTypeDecl(resultType.value());
    if (resultDecl && libadalang::isArrayTypeDecl(resultDecl.value())) {
      mlir::emitError(loc(resultType.value()),
                      "returning an array is not supported");
      return nullptr;
    }
  }

  std::optional<llvm::SmallVector<SubpParam, 4>> params =
      subpParams(ada_subp_spec);
  if (!params)
    return nullptr;
  auto subp = mlirGenSubpSpec(ada_subp_spec, resultType, *params);
  if (!subp)
    return nullptr;
  // Enter the subprogram's naming scope for the declarations in its body.
  auto scopeGuard = pushScope(subp.getSymName().str());

  // Neither a loop nor a goto label is reachable across a subprogram boundary
  // (@rm{5-7}, @rm{5-8}), so both are subprogram-local: clear them for this
  // body (a nested subp may be emitted inside an enclosing loop) and restore
  // on exit. This differs from `scopeStack`, which stays cumulative for
  // nested qualified names.
  auto savedLoops = std::move(loopStack);
  auto savedLabels = std::move(labelBlocks);
  auto savedLabelMarkers = std::move(pendingLabels);
  loopStack.clear();
  labelBlocks.clear();
  pendingLabels.clear();
  auto frameGuard = llvm::scope_exit([&] {
    loopStack = std::move(savedLoops);
    labelBlocks = std::move(savedLabels);
    pendingLabels = std::move(savedLabelMarkers);
  });
  mlir::Block *entryBlock = &subp.getBody().front();

  builder.setInsertionPointToStart(entryBlock);

  // Bind source formals. Writable formals denote independent objects whose
  // initial and final values are transferred by LowerAdaCalls.
  for (auto [entry, arg] : llvm::zip(*params, entryBlock->getArguments())) {
    arg.setLoc(mlir::NameLoc::get(getNameAttr(entry.id), loc(entry.id)));
    declare(entry.id, arg);
  }

  // ada_null_subp_decl and ada_expr_function don't have declarations nor
  // statements, so do not visit decls nor stmts in that case.
  ada_node decls = {};
  ada_subp_body_f_decls(&subp_body, &decls);
  if (!ada_node_is_null(&decls))
    if (mlir::failed(mlirGenDeclarativePart(decls)))
      return nullptr;

  ada_node stmts = {};
  ada_subp_body_f_stmts(&subp_body, &stmts);

  if (!ada_node_is_null(&stmts) && mlir::failed(visit(stmts))) {
    subp.erase();
    return nullptr;
  }

  // Procedures can have no explicit return statement, so add an implicit one
  // unless the body already ended in a terminator. It is located at the end
  // name, if any, where a debugger stops last.
  if (subp.isProcedure() && !currentBlockTerminated()) {
    ada_node endName = {};
    mlir::Location retLoc = ada_subp_body_f_end_name(&subp_body, &endName) &&
                                    !ada_node_is_null(&endName)
                                ? loc(endName)
                                : mlir::UnknownLoc::get(builder.getContext());
    mlir::ada::ReturnOp::create(builder, retLoc, mlir::Value{});
  }

  // Expr function has one expression to visit and to return.
  if (ada_node_kind(&subp_body) == ada_expr_function) {
    ada_node expr = {};
    ada_expr_function_f_expr(&subp_body, &expr);

    if (ada_node_is_null(&expr)) {
      mlir::emitError(loc(subp_body),
                      "can't get the expression of the expression function");
      subp.erase();
      return nullptr;
    }

    mlir::FailureOr<mlir::Value> retVal = mlirGenReturnExpr(expr, subp);

    if (mlir::failed(retVal)) {
      subp.erase();
      return nullptr;
    }

    mlir::ada::ReturnOp::create(builder, loc(expr), retVal.value());
  }

  anchorPendingLabels();
  return subp;
}

mlir::LogicalResult MLIRGenImpl::mlirGenPackageDecl(ada_node &decl) {
  builder.setInsertionPointToStart(adaModule.getBody());
  std::optional<ada_node> def_name = libadalang::declDefiningName(decl);
  if (!def_name)
    return mlir::failure();
  auto scopeGuard = pushScope(canonicalFqn(*def_name));

  ada_node public_part = {}, private_part = {};

  if (!ada_base_package_decl_f_public_part(&decl, &public_part) ||
      ada_node_is_null(&public_part) ||
      mlir::failed(mlirGenDeclarativePart(public_part)))
    return mlir::failure();

  if (!ada_base_package_decl_f_private_part(&decl, &private_part))
    return mlir::failure();

  // The private part of a package is null when not specified.
  if (!ada_node_is_null(&private_part))
    return mlirGenDeclarativePart(private_part);

  return mlir::success();
}

mlir::LogicalResult MLIRGenImpl::mlirGenPackageBody(ada_node &body) {
  builder.setInsertionPointToStart(adaModule.getBody());
  std::optional<ada_node> def_name = libadalang::declDefiningName(body);
  if (!def_name)
    return mlir::failure();
  auto scopeGuard = pushScope(canonicalFqn(*def_name));

  // Process the spec of that body.
  std::optional<ada_node> spec = libadalang::declPart(body);

  if (!spec)
    return mlir::emitError(loc(*def_name), "specification of package ")
           << libadalang::getName(&*def_name, /*canonical=*/false)
           << " not found";
  if (libadalang::emitParserDiagnostics(spec.value()))
    return mlir::failure();
  if (mlir::failed(mlirGenPackageDecl(spec.value())))
    return mlir::failure();

  ada_node declarative_part = {};

  if (!ada_package_body_f_decls(&body, &declarative_part) ||
      ada_node_is_null(&declarative_part) ||
      mlir::failed(mlirGenDeclarativePart(declarative_part)))
    return mlir::failure();

  return mlir::success();
}

std::optional<llvm::SmallVector<MLIRGenImpl::SubpParam, 4>>
MLIRGenImpl::subpParams(ada_node &subp_spec) {
  ada_node_array params = nullptr;
  if (!ada_base_subp_spec_p_params(&subp_spec, &params) || !params) {
    mlir::emitError(loc(subp_spec), "failed to get subprogram parameters");
    return std::nullopt;
  }
  auto paramsGuard = llvm::scope_exit([&] { ada_node_array_dec_ref(params); });

  llvm::SmallVector<SubpParam, 4> result;
  for (int i = 0; i < params->n; i++) {
    ada_node aliased;
    ada_param_spec_f_has_aliased(&params->items[i], &aliased);
    if (ada_node_kind(&aliased) == ada_aliased_present) {
      mlir::emitError(loc(aliased),
                      "explicitly aliased parameters are not supported");
      return std::nullopt;
    }
    ada_node mode_node, type_expr, ids;
    ada_param_spec_f_mode(&params->items[i], &mode_node);
    ada_param_spec_f_type_expr(&params->items[i], &type_expr);
    ada_param_spec_f_ids(&params->items[i], &ids);
    for (unsigned j = 0; j < ada_node_children_count(&ids); j++) {
      ada_node child;
      if (ada_node_child(&ids, j, &child) == 0 || ada_node_is_null(&child)) {
        mlir::emitError(loc(ids), "failed to get parameter identifier");
        return std::nullopt;
      }
      result.push_back({child, ada_node_kind(&mode_node), type_expr});
    }
  }
  return result;
}

mlir::ada::SubpOp
MLIRGenImpl::mlirGenSubpSpec(ada_node &subp_spec,
                             std::optional<ada_node> resultType,
                             llvm::ArrayRef<SubpParam> params) {
  auto location = loc(libadalang::parent(&subp_spec));

  ada_node name;
  ada_subp_spec_f_subp_name(&subp_spec, &name);

  llvm::SmallVector<mlir::Type, 4> argTypes;
  for (SubpParam param : params) {
    mlir::ada::QualType paramType = getAdaQualType(param.typeExpr);
    if (!paramType)
      return nullptr;
    bool writable = param.mode == ada_mode_in_out || param.mode == ada_mode_out;
    argTypes.push_back(writable ? mlir::Type(mlir::ada::RefType::get(
                                      builder.getContext(), paramType))
                                : mlir::Type(paramType));
  }

  llvm::SmallVector<mlir::Type, 1> retTypes;
  if (resultType) {
    mlir::ada::QualType retType = getAdaQualType(resultType.value());
    if (!retType)
      return nullptr;
    retTypes.push_back(retType);
  }
  auto funcType = builder.getFunctionType(argTypes, retTypes);
  ada_node canon = canonicalDefName(name);
  std::string symName =
      declareSymbol(canon, libadalang::getName(&name), /*useFqn=*/false);
  auto subpOp = mlir::ada::SubpOp::create(builder, location, symName, funcType);
  // Nested subprograms are not externally visible: mark them private so they
  // skip the GNAT `_ada_` prefix (`getMangledName`). Library-level
  // subprograms (emitted directly under the module) keep the default public
  // visibility.
  if (!mlir::isa<mlir::ModuleOp>(subpOp->getParentOp()))
    subpOp.setPrivate();
  for (auto [i, param] : llvm::enumerate(params)) {
    subpFormals[subpOp].push_back(param.id);
    if (param.mode == ada_mode_default)
      continue;
    auto mode = mlir::ada::AdaParamMode::In;
    if (param.mode == ada_mode_out)
      mode = mlir::ada::AdaParamMode::Out;
    else if (param.mode == ada_mode_in_out)
      mode = mlir::ada::AdaParamMode::InOut;
    subpOp.setArgAttr(
        i, "ada.mode",
        mlir::ada::AdaParamModeAttr::get(builder.getContext(), mode));
  }
  subpDecls[canon.node] = subpOp;

  return subpOp;
}

} // namespace lalvm::mlirgen
