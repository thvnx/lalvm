//===- HoistNestedSymbolOperations.cpp - Hoist nested Ada symbol ops ------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the HoistNestedSymbolOperationsPass. Closure conversion
// has already hoisted nested ada.subp ops to module level, so this pass applies
// GNAT-style ABI name mangling to every subprogram in place. It also lifts the
// remaining nested ada.type ops to module level so that their Symbol trait
// stays valid (parent is always the module's SymbolTable) through lowering,
// where they survive inside no llvm.func body, and erases the emptied
// ada.decls.
//
//===----------------------------------------------------------------------===//

#include "ada/Dialect.h"
#include "ada/Passes.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Location.h"
#include "mlir/IR/SymbolTable.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Support/TypeID.h"

using namespace mlir;

//===----------------------------------------------------------------------===//
// HoistNestedSymbolOperationsPass
//===----------------------------------------------------------------------===//

namespace {
struct HoistNestedSymbolOperationsPass
    : public PassWrapper<HoistNestedSymbolOperationsPass,
                         OperationPass<ModuleOp>> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(HoistNestedSymbolOperationsPass)
  void runOnOperation() final;
  StringRef getName() const final { return "HoistNestedSymbolOperations"; }
  StringRef getArgument() const final { return "ada-hoist-nested-symbols"; }
  StringRef getDescription() const final {
    return "Apply GNAT ABI mangling and hoist nested types to module level";
  }
};
} // namespace

void HoistNestedSymbolOperationsPass::runOnOperation() {
  // Hoist types before mangling subprograms so their source scopes are intact.
  ModuleOp module = getOperation();
  MLIRContext *ctx = module.getContext();
  llvm::SmallVector<std::pair<Operation *, std::string>, 4> subps;
  llvm::SmallVector<ada::DeclsOp, 4> declsOps;
  auto insertion = module.getBody()->begin();
  module.walk([&](Operation *op) {
    // ada.decls now holds at most ada.type ops (closure conversion hoisted the
    // subprograms out); collect them to erase once their types are lifted.
    if (auto decls = dyn_cast<ada::DeclsOp>(op)) {
      declsOps.push_back(decls);
      return;
    }
    // ada.type carries the Symbol trait, so its parent must be a SymbolTable at
    // every stage. Hoist nested ones to module level so none survive inside a
    // non-SymbolTable llvm.func body after lowering. No mangling: type symbols
    // are not part of the GNAT ABI, and FQN sym_names are already unique.
    if (isa<ada::TypeOp>(op)) {
      if (op->getParentOfType<ModuleOp>() != module)
        return;
      if (!isa<ModuleOp>(op->getParentOp())) {
        // Preserve the source scope before moving the type away from it.
        if (auto subp = op->getParentOfType<ada::SubpOp>())
          op->setLoc(mlir::FusedLoc::get(
              ctx, {op->getLoc()},
              ada::DIScopeRefAttr::get(ctx, mlir::FlatSymbolRefAttr::get(
                                                ctx, subp.getMangledName()))));
      }
      // Extend the leading type group in walk order. Already placed types
      // advance the cursor without being moved or revisited.
      if (op->getBlock() == module.getBody() && op->getIterator() == insertion)
        ++insertion;
      else
        op->moveBefore(module.getBody(), insertion);
      return;
    }
    if (auto subp = dyn_cast<ada::SubpOp>(op))
      subps.emplace_back(op, subp.getMangledName());
  });
  // Mangle every subprogram in place (closure conversion already moved the
  // nested ones to module level). getMangledName reads visibility rather than
  // the op's parent, so library-level (public, `_ada_`) and nested (private)
  // subprograms still mangle correctly. replaceAllSymbolUses walks the whole
  // module region (including each subprogram's own body), so self-recursive
  // calls are rewritten too.
  for (auto &[op, mangledName] : subps) {
    // Capture the bare Ada source name before the sym_name is overwritten with
    // the mangled form (after which it is no longer recoverable: the `__`
    // segment separator collides with MLIRGen's `__N` collision suffix). Fuse
    // it onto the location so AdaDebugInfoPass can set DW_AT_name to the source
    // name while DW_AT_linkage_name keeps the mangled symbol.
    auto bareNameAttr = mlir::StringAttr::get(
        ctx, ada::bareName(cast<ada::SubpOp>(op).getSymName()));
    op->setLoc(mlir::FusedLoc::get(ctx, {op->getLoc()},
                                   ada::DINameAttr::get(ctx, bareNameAttr)));

    auto mangledAttr = mlir::StringAttr::get(ctx, mangledName);
    if (mlir::failed(
            mlir::SymbolTable::replaceAllSymbolUses(op, mangledAttr, module)))
      return signalPassFailure();
    mlir::SymbolTable::setSymbolName(op, mangledName);
  }
  // Closure conversion erased the subprogram-only ada.decls; the rest held
  // types, now lifted out, so every remaining container is empty. Erase them.
  for (ada::DeclsOp decls : declsOps) {
    if (!decls.getBody().front().empty()) {
      decls.emitError("ada.decls is not empty after hoisting nested symbols");
      return signalPassFailure();
    }
    decls.erase();
  }
}

/// Create a pass that GNAT-mangles subprograms in place and hoists the
/// remaining nested Ada `ada.type` ops to module level, erasing the emptied
/// `ada.decls`.
std::unique_ptr<mlir::Pass> mlir::ada::createHoistNestedSymbolOperationsPass() {
  return std::make_unique<HoistNestedSymbolOperationsPass>();
}
