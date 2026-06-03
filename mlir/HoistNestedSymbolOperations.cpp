//===- HoistNestedSymbolOperations.cpp - Hoist nested Ada symbol ops ------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the HoistNestedSymbolOperationsPass, which lifts all
// nested ada.subp ops to module level and applies GNAT-style ABI name
// mangling. It also lifts all ada.type ops to module level so that their
// Symbol trait stays valid (parent is always the module's SymbolTable) through
// lowering, where they survive inside no llvm.func body.
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
  // Two-phase ABI renaming before lowering. All ops are collected in a single
  // walk before any mutations so the parent chain is still intact.
  //
  // Phase 1: hoist nested subprograms: LLVM does not support nested
  // functions. Each nested op is renamed with GNAT-style __ separators built
  // from the full enclosing scope chain (e.g. @inner inside @outer becomes
  // @outer__inner). Parent names at this point are still bare Ada names, so
  // the mangling matches GNAT (outer__inner, not _ada_outer__inner).
  //
  // Phase 2: apply _ada_ prefix: library-level subprograms get the GNAT
  // _ada_ prefix. Done after hoisting so nested mangling uses bare names.
  ModuleOp module = getOperation();
  llvm::SmallVector<std::pair<Operation *, std::string>, 4> nestedSubps;
  llvm::SmallVector<Operation *, 4> librarySubps;
  llvm::SmallVector<std::pair<Operation *, std::string>, 4> nestedTypes;
  llvm::SmallVector<ada::DeclsOp, 4> declsOps;
  // Collect all subprograms and types in one walk before any mutations. The
  // walk completes fully before the loops below mutate the IR (moveBefore,
  // setSymbolName), so there is no iterator invalidation.
  module.walk([&](Operation *op) {
    // ada.decls only holds nested subprograms/types, all hoisted to module
    // level below. Collect the now-redundant containers to erase afterward.
    if (auto decls = dyn_cast<ada::DeclsOp>(op)) {
      declsOps.push_back(decls);
      return;
    }
    // ada.type carries the Symbol trait, so its parent must be a SymbolTable at
    // every stage. Hoist nested ones to module level alongside subprograms so
    // none survive inside a non-SymbolTable llvm.func body after lowering. No
    // mangling: type symbols are not part of the GNAT ABI, and FQN sym_names
    // are already unique, so a plain move suffices.
    if (isa<ada::TypeOp>(op)) {
      if (!isa<ModuleOp>(op->getParentOp())) {
        // Capture the enclosing subprogram's mangled name now, while FQN
        // sym_names are still intact (the rename loops run afterward). It is
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
    if (!isa<ada::SubpOp>(op))
      return;
    if (isa<ModuleOp>(op->getParentOp())) {
      librarySubps.push_back(op);
      return;
    }
    nestedSubps.emplace_back(op, cast<ada::SubpOp>(op).getMangledName());
  });
  // Hoist every nested subprogram to module level first, keeping its FQN
  // dialect name. While nested inside an `ada.decls` (a SymbolTable that is a
  // sibling of the calling statements, not an ancestor), a call in the
  // enclosing body cannot resolve to it, so renaming uses would miss the call
  // sites. Once at module level, every call resolves by the standard upward
  // symbol-table walk.
  for (auto &[op, mangledName] : nestedSubps)
    op->moveBefore(module.getBody(), module.getBody()->end());
  // Now rename with the standard utility. With the subprogram directly under
  // the module, `replaceAllSymbolUses` walks the whole module region, which
  // includes the subprogram's own body, so self-recursive calls are rewritten
  // too.
  for (auto &[op, mangledName] : nestedSubps) {
    auto mangledAttr = mlir::StringAttr::get(module.getContext(), mangledName);
    if (mlir::failed(
            mlir::SymbolTable::replaceAllSymbolUses(op, mangledAttr, module)))
      return signalPassFailure();
    mlir::SymbolTable::setSymbolName(op, mangledName);
  }
  for (Operation *op : librarySubps) {
    std::string mangledName = cast<ada::SubpOp>(op).getMangledName();
    auto mangledAttr = mlir::StringAttr::get(module.getContext(), mangledName);
    if (mlir::failed(
            mlir::SymbolTable::replaceAllSymbolUses(op, mangledAttr, module)))
      return signalPassFailure();
    mlir::SymbolTable::setSymbolName(op, mangledName);
  }
  // Hoist nested ada.type ops to module level. Their FQN sym_names are unique,
  // so no rename or symbol-use rewrite is needed: a plain move keeps every
  // type-symbol reference (the @sym in ada.qual) valid. Fuse the captured
  // enclosing subprogram onto the location so EnumDITypes can still place the
  // type's DWARF scope under that subprogram after the move.
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
  // Every nested subprogram and type has been lifted to module level, so each
  // ada.decls is now empty; erase the containers. Done last so the moves above
  // (which read parent chains crossing these ops) are unaffected.
  for (ada::DeclsOp decls : declsOps) {
    if (!decls.getBody().front().empty()) {
      decls.emitError("ada.decls is not empty after hoisting nested symbols");
      return signalPassFailure();
    }
    decls.erase();
  }
}

/// Create a pass that hoists nested Ada symbol operations (subprograms and
/// types) to module level and applies GNAT ABI name mangling to subprograms.
std::unique_ptr<mlir::Pass> mlir::ada::createHoistNestedSymbolOperationsPass() {
  return std::make_unique<HoistNestedSymbolOperationsPass>();
}
