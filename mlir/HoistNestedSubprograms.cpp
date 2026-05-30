//===- HoistNestedSubprograms.cpp - Hoist nested Ada subprograms ----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the AdaHoistNestedSubprogramsPass, which lifts all
// nested ada.subp ops to module level and applies GNAT-style ABI name
// mangling.
//
//===----------------------------------------------------------------------===//

#include "ada/Dialect.h"
#include "ada/Passes.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/SymbolTable.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Support/TypeID.h"

using namespace mlir;

//===----------------------------------------------------------------------===//
// AdaHoistNestedSubprogramsPass
//===----------------------------------------------------------------------===//

namespace {
struct AdaHoistNestedSubprogramsPass
    : public PassWrapper<AdaHoistNestedSubprogramsPass,
                         OperationPass<ModuleOp>> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(AdaHoistNestedSubprogramsPass)
  void runOnOperation() final;
};
} // namespace

void AdaHoistNestedSubprogramsPass::runOnOperation() {
  // Two-phase ABI renaming before lowering. Both sets of ops are collected in
  // a single walk before any mutations so the parent chain is still intact.
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
  // Collect all subprograms in one walk before any mutations. The walk
  // completes fully before the loops below mutate the IR (moveBefore,
  // setSymbolName), so there is no iterator invalidation.
  module.walk([&](Operation *op) {
    if (!isa<ada::SubpOp>(op))
      return;
    if (isa<ModuleOp>(op->getParentOp())) {
      librarySubps.push_back(op);
      return;
    }
    nestedSubps.emplace_back(op, cast<ada::SubpOp>(op).getMangledName());
  });
  for (auto &[op, mangledName] : nestedSubps) {
    auto mangledAttr = mlir::StringAttr::get(module.getContext(), mangledName);
    // Save the old name before any mutation.
    std::string oldName = mlir::SymbolTable::getSymbolName(op).str();
    if (mlir::failed(
            mlir::SymbolTable::replaceAllSymbolUses(op, mangledAttr, module)))
      return signalPassFailure();
    // replaceAllSymbolUses stops at the symbol's own definition body, so
    // self-recursive calls inside 'op' are left unrenamed.  Fix that with a
    // targeted walk that crosses SymbolTable boundaries.
    auto mangledSymRef =
        mlir::FlatSymbolRefAttr::get(module.getContext(), mangledName);
    op->walk([&](ada::CallOp callOp) {
      if (callOp.getCallee() == oldName)
        callOp.setCalleeAttr(mangledSymRef);
    });
    mlir::SymbolTable::setSymbolName(op, mangledName);
    op->moveBefore(module.getBody(), module.getBody()->end());
  }
  for (Operation *op : librarySubps) {
    std::string mangledName = cast<ada::SubpOp>(op).getMangledName();
    auto mangledAttr = mlir::StringAttr::get(module.getContext(), mangledName);
    if (mlir::failed(
            mlir::SymbolTable::replaceAllSymbolUses(op, mangledAttr, module)))
      return signalPassFailure();
    mlir::SymbolTable::setSymbolName(op, mangledName);
  }
}

/// Create a pass that hoists nested Ada subprograms to module level and
/// applies GNAT ABI name mangling.
std::unique_ptr<mlir::Pass> mlir::ada::createHoistNestedSubprogramsPass() {
  return std::make_unique<AdaHoistNestedSubprogramsPass>();
}
