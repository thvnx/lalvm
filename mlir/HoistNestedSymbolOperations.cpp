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
};
} // namespace

void HoistNestedSymbolOperationsPass::runOnOperation() {
  // All ops are collected in one walk before any mutation, so the scope-name
  // capture below reads intact FQN sym_names and there is no iterator
  // invalidation.
  ModuleOp module = getOperation();
  llvm::SmallVector<std::pair<Operation *, std::string>, 4> subps;
  llvm::SmallVector<std::pair<Operation *, std::string>, 4> nestedTypes;
  llvm::SmallVector<ada::DeclsOp, 4> declsOps;
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
      if (!isa<ModuleOp>(op->getParentOp())) {
        // Capture the enclosing subprogram's mangled name now, while FQN
        // sym_names are still intact (the rename loop runs afterward). It is
        // fused onto the type's location below as its DWARF scope, recovered in
        // EnumDITypes once the op has been lifted away from its lexical parent.
        std::string scopeName;
        for (Operation *p = op->getParentOp(); p; p = p->getParentOp())
          if (auto subp = dyn_cast<ada::SubpOp>(p)) {
            scopeName = subp.getMangledName();
            break;
          }
        nestedTypes.emplace_back(op, std::move(scopeName));
      }
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
    auto mangledAttr = mlir::StringAttr::get(module.getContext(), mangledName);
    if (mlir::failed(
            mlir::SymbolTable::replaceAllSymbolUses(op, mangledAttr, module)))
      return signalPassFailure();
    mlir::SymbolTable::setSymbolName(op, mangledName);
  }
  // Hoist the remaining nested ada.type ops to module level. Their FQN
  // sym_names are unique, so a plain move keeps every type-symbol reference
  // (the @sym in ada.qual) valid. Fuse the captured enclosing subprogram onto
  // the location so EnumDITypes can still place the type's DWARF scope under
  // it.
  for (auto &[op, scopeName] : nestedTypes) {
    if (!scopeName.empty()) {
      auto *ctx = module.getContext();
      op->setLoc(mlir::FusedLoc::get(
          ctx, {op->getLoc()},
          ada::DIScopeRefAttr::get(
              ctx, mlir::FlatSymbolRefAttr::get(ctx, scopeName))));
    }
    op->moveBefore(module.getBody(), module.getBody()->end());
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
