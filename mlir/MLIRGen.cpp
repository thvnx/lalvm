// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2024-2026 The LALVM Project

#include "ada/MLIRGen.h"
#include "MLIRGenImpl.h"
#include "frontend/DiagnosticPrinter.h"
#include "mlir/IR/Diagnostics.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/IR/Verifier.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"

namespace libadalang = frontend::libadalang;

#define DEBUG_TYPE "ada-mlirgen"

namespace lalvm::mlirgen {

mlir::ModuleOp MLIRGenImpl::mlirGen(ada_node &compilationUnit) {
  frontend::ScopedDiagnosticPrinter diagPrinter(builder.getContext());

  // We create an empty MLIR module and walk the entire compilation unit to
  // codegen its contents into it.
  std::string filename = libadalang::unitFilename(compilationUnit);
  adaModule = mlir::ModuleOp::create(loc(compilationUnit),
                                     llvm::sys::path::stem(filename));

  // Standard input has no file name to match the unit name against.
  if (llvm::sys::path::filename(filename) != libadalang::kStdinName)
    verifyUnitFileName(compilationUnit);

  // Use a simple Libadalang AST traversal approach based on the C API.
  if (mlir::failed(visit(compilationUnit)))
    return nullptr;

  // Verify the module after we have finished constructing it, this will check
  // the structural properties of the IR and invoke any specific verifiers we
  // have on the Ada operations.
  if (failed(mlir::verify(adaModule))) {
    adaModule.emitError("module verification error");
    return nullptr;
  }

  return adaModule;
}

mlir::StringAttr MLIRGenImpl::getNameAttr(ada_node &node) {
  return builder.getStringAttr(libadalang::getName(&node));
}

void MLIRGenImpl::setAdaNameLoc(mlir::Operation *op, mlir::StringAttr name) {
  op->setLoc(mlir::NameLoc::get(name, op->getLoc()));
}

void MLIRGenImpl::setAdaNameLoc(mlir::Value val, mlir::StringAttr name) {
  setAdaNameLoc(val, name, val.getLoc());
}

void MLIRGenImpl::setAdaNameLoc(mlir::Value val, mlir::StringAttr name,
                                mlir::Location srcLoc) {
  val.setLoc(mlir::NameLoc::get(name, srcLoc));
}

mlir::Location MLIRGenImpl::propagateAdaNameLoc(mlir::Value ptr,
                                                mlir::Location srcLoc) {
  auto nameLoc = mlir::dyn_cast<mlir::NameLoc>(ptr.getLoc());
  if (!nameLoc)
    return srcLoc;
  return mlir::NameLoc::get(nameLoc.getName(), srcLoc);
}

void MLIRGenImpl::verifyUnitFileName(ada_node &node) {
  // Precondition: adaModule has been created with a name by the caller.
  ada_symbol_type_array fqn = nullptr;
  if (!ada_compilation_unit_p_syntactic_fully_qualified_name(&node, &fqn))
    return;
  if (!fqn || fqn->n == 0) {
    ada_symbol_type_array_dec_ref(fqn);
    return;
  }
  ada_text nameText;
  ada_symbol_text(&fqn->items[fqn->n - 1], &nameText);
  std::string unitName = libadalang::textToString(nameText);
  ada_symbol_type_array_dec_ref(fqn);

  ada_analysis_unit_kind kind;
  if (!ada_compilation_unit_p_unit_kind(&node, &kind))
    return;
  llvm::StringRef ext =
      (kind == ADA_ANALYSIS_UNIT_KIND_UNIT_BODY) ? ".adb" : ".ads";

  if (adaModule.getName()->lower() != unitName)
    mlir::emitWarning(loc(node),
                      "file name does not match unit name, should be \"" +
                          llvm::Twine(unitName) + ext + "\"");
}

ada_node MLIRGenImpl::canonicalDefName(ada_node defName) {
  ada_node decl, canonDecl, canonName;
  if (!ada_defining_name_p_basic_decl(&defName, &decl) ||
      ada_node_is_null(&decl))
    return defName;
  if (!ada_basic_decl_p_canonical_part(&decl, /*imprecise_fallback=*/false,
                                       &canonDecl) ||
      ada_node_is_null(&canonDecl))
    return defName;
  if (!ada_basic_decl_p_defining_name(&canonDecl, &canonName) ||
      ada_node_is_null(&canonName))
    return defName;
  return canonName;
}

std::string MLIRGenImpl::makeUnique(std::string candidate) {
  if (usedSymbols.insert(candidate).second)
    return candidate;
  for (unsigned n = 2;; ++n) {
    std::string c = candidate + "__" + std::to_string(n);
    if (usedSymbols.insert(c).second)
      return c;
  }
}

std::string MLIRGenImpl::scopePrefixForInsertion() {
  return scopeStack.empty() ? std::string() : scopeStack.back();
}

std::string MLIRGenImpl::makeUniqueInScope(llvm::StringRef simple) {
  std::string prefix = scopePrefixForInsertion();
  return makeUnique(prefix.empty() ? simple.str()
                                   : prefix + "." + simple.str());
}

std::string MLIRGenImpl::canonicalFqn(ada_node defName) {
  if (std::optional<std::string> fqn =
          libadalang::canonicalFullyQualifiedName(defName))
    return *fqn;
  return libadalang::getName(&defName, /*canonical=*/true);
}

std::string MLIRGenImpl::declareSymbol(ada_node canonDef,
                                       llvm::StringRef simple, bool useFqn) {
  if (auto it = symbolNames.find(canonDef.node); it != symbolNames.end())
    return it->second;
  std::string name;
  if (useFqn)
    name = canonicalFqn(canonDef);
  else
    name = makeUniqueInScope(simple);
  symbolNames[canonDef.node] = name;
  return name;
}

std::string MLIRGenImpl::resolveSymbol(ada_node refDefName) {
  ada_node canon = canonicalDefName(refDefName);
  if (auto it = symbolNames.find(canon.node); it != symbolNames.end())
    return it->second;
  return canonicalFqn(canon);
}

mlir::Location MLIRGenImpl::loc(const ada_node &node) {
  return libadalang::sourceLocation(*builder.getContext(), node);
}

void MLIRGenImpl::declare(ada_node &def_name, mlir::Value value) {
  LLVM_DEBUG(llvm::dbgs() << "declare: " << libadalang::image(&def_name)
                          << "\n");
  declValues[def_name.node] = value;
}

mlir::LogicalResult MLIRGenImpl::checkResolution(ada_node &node) {
  ada_bool isEntryPoint = 0;
  if (ada_ada_node_p_xref_entry_point(&node, &isEntryPoint) && isEntryPoint &&
      libadalang::emitSolverDiagnostics(&node))
    return mlir::failure();
  return mlir::success();
}

mlir::LogicalResult MLIRGenImpl::visit(ada_node &node) {
  if (mlir::failed(checkResolution(node)))
    return mlir::failure();

  switch (ada_node_kind(&node)) {
  case ada_subp_body:
    // Top-level subprograms are emitted at module scope. The insertion point
    // is set here rather than inside mlirGenSubpBody so that nested
    // subprograms (processed via mlirGenDeclarativePart) are instead emitted
    // at the current insertion point inside the enclosing body region.
    builder.setInsertionPointToEnd(adaModule.getBody());
    if (!mlirGenSubpBody(node))
      return mlir::failure();
    return mlir::success();
  case ada_null_subp_decl:
  case ada_expr_function: {
    // Null procedure and expression functions can't be library units (see
    // @rm{10-1-1}).
    ada_node parent = libadalang::parent(&node);
    if (ada_node_kind(&parent) == ada_library_item)
      return mlir::emitError(loc(node),
                             ada_node_kind(&node) == ada_null_subp_decl
                                 ? "null procedure cannot be a library unit"
                                 : "expression function cannot be a library "
                                   "unit");
    return mlirGenSubpBody(node) ? mlir::success() : mlir::failure();
  }
  case ada_return_stmt:
    return mlirGenReturn(node);
  case ada_assign_stmt:
    return mlirGenAssign(node);
  case ada_null_stmt:
    mlir::ada::NullOp::create(builder, loc(node));
    return mlir::success();
  case ada_call_stmt:
    return mlirGenCallStmt(node);
  case ada_if_stmt:
    return mlirGenIf(node);
  // `while`, bare `loop`, and `for` are distinct node kinds, all deriving
  // from BaseLoopStmt (@rm{5-5}).
  case ada_loop_stmt:
  case ada_while_loop_stmt:
  case ada_for_loop_stmt:
    return mlirGenLoop(node, {});
  case ada_exit_stmt:
    return mlirGenExit(node);
  case ada_goto_stmt:
    return mlirGenGoto(node);
  case ada_label:
    return mlirGenLabel(node);
  case ada_named_stmt: {
    // Named statement: "Name: ... end Name;". The name lives on the wrapping
    // named_stmt; the actual statement is f_stmt: a loop or a block.
    ada_node decl, nameNode, stmt;
    ada_named_stmt_f_decl(&node, &decl);
    ada_named_stmt_decl_f_name(&decl, &nameNode);
    ada_named_stmt_f_stmt(&node, &stmt);
    std::string name = libadalang::getName(&nameNode);
    switch (ada_node_kind(&stmt)) {
    case ada_loop_stmt:
    case ada_while_loop_stmt:
    case ada_for_loop_stmt:
      return mlirGenLoop(stmt, name);
    default:
      return mlirGenBlock(stmt, name);
    }
  }
  case ada_begin_block:
  case ada_decl_block:
    return mlirGenBlock(node, {});
  case ada_with_clause:
  case ada_use_package_clause:
  case ada_use_type_clause:
    // Context clauses (@rm{10-1-2}): resolved by libadalang, no codegen.
    // Return without descending, so their names do not warn as unhandled.
    return mlir::success();
  case ada_package_decl:
    return mlirGenPackageDecl(node);
  case ada_package_body:
    return mlirGenPackageBody(node);
  case ada_compilation_unit:
  case ada_ada_node_list:
  case ada_library_item:
  case ada_private_absent:
  case ada_private_present:
  case ada_pragma_node_list:
  case ada_handled_stmts:
  case ada_stmt_list:
    // Transparent nodes: visit children without warning.
    break;
  default: {
    // @todo Turn this into an error when lalvm is mature enough.
    mlir::emitWarning(loc(node), "visit: unhandled node '")
        << libadalang::image(&node) << "'";
    break;
  }
  }

  // Only a statement sequence threads a single block across its children; in
  // other contexts (e.g. a list of declarations) a terminated insertion block
  // is an artifact of the previously emitted op, not unreachable code.
  bool inStmtList = ada_node_kind(&node) == ada_stmt_list;
  bool warnedDead = false;
  unsigned i, count = ada_node_children_count(&node);
  for (i = 0; i < count; ++i) {
    ada_node child;
    if (ada_node_child(&node, i, &child) == 0) {
      mlir::emitError(loc(node), "failed to get child node");
      return mlir::failure();
    }
    if (ada_node_is_null(&child))
      continue;
    // A terminator (return, goto) ends its block, so following statements are
    // unreachable, except a label: a live branch target that reopens the flow
    // (@rm{5-8}). Visit labels; warn once and skip the rest.
    if (inStmtList && currentBlockTerminated() &&
        ada_node_kind(&child) != ada_label) {
      if (!warnedDead) {
        mlir::emitWarning(loc(child), "unreachable code");
        warnedDead = true;
      }
      continue;
    }
    warnedDead = false;
    if (mlir::failed(visit(child)))
      return mlir::failure();
  }
  return mlir::success();
}

mlir::Value MLIRGenImpl::unwrap(mlir::Value value, mlir::Location location) {
  auto qual = mlir::dyn_cast<mlir::ada::QualType>(value.getType());
  if (!qual) {
    mlir::emitError(location, "cannot unwrap non-qualified value of type ")
        << value.getType();
    return nullptr;
  }
  auto *ctx = builder.getContext();
  mlir::Location fused = mlir::FusedLoc::get(
      ctx, {location}, mlir::ada::DITypeRefAttr::get(ctx, qual.getAdaType()));
  return mlir::ada::UnwrapOp::create(builder, fused, qual.getMlirType(), value);
}

std::optional<llvm::SmallVector<MLIRGenImpl::IfGuard, 4>>
MLIRGenImpl::collectIfGuards(ada_node &ifNode) {
  bool isExpr = ada_node_kind(&ifNode) == ada_if_expr;
  ada_node cond, then, alternatives;
  if (isExpr) {
    ada_if_expr_f_cond_expr(&ifNode, &cond);
    ada_if_expr_f_then_expr(&ifNode, &then);
    ada_if_expr_f_alternatives(&ifNode, &alternatives);
  } else {
    ada_if_stmt_f_cond_expr(&ifNode, &cond);
    ada_if_stmt_f_then_stmts(&ifNode, &then);
    ada_if_stmt_f_alternatives(&ifNode, &alternatives);
  }
  llvm::SmallVector<IfGuard, 4> guards{{cond, then}};

  unsigned nalt = ada_node_children_count(&alternatives);
  for (unsigned i = 0; i < nalt; ++i) {
    ada_node part;
    if (ada_node_child(&alternatives, i, &part) == 0 ||
        ada_node_is_null(&part)) {
      mlir::emitError(loc(ifNode), "failed to get elsif part");
      return std::nullopt;
    }
    if (isExpr) {
      ada_elsif_expr_part_f_cond_expr(&part, &cond);
      ada_elsif_expr_part_f_then_expr(&part, &then);
    } else {
      ada_elsif_stmt_part_f_cond_expr(&part, &cond);
      ada_elsif_stmt_part_f_stmts(&part, &then);
    }
    guards.push_back({cond, then});
  }
  return guards;
}

} // namespace lalvm::mlirgen

namespace lalvm {

mlir::OwningOpRef<mlir::ModuleOp> mlirGen(mlir::MLIRContext &context,
                                          ada_node &compilationUnit) {
  return mlirgen::MLIRGenImpl(context).mlirGen(compilationUnit);
}

} // namespace lalvm
