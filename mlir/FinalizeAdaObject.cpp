//===- FinalizeAdaObject.cpp - Emit LLVM debug intrinsics from NameLoc ----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass emits LLVM debug intrinsics for Ada objects. It runs after
// DIScopeForLLVMFuncOpPass so that DISubprogramAttr is available on each
// llvm.func.
//
// Objects are identified by their NameLoc, set by MLIRGen on:
//   - memref.alloca (ObjectDecl): lowered to llvm.alloca
//   - arith.constant init expr (ObjectDecl with init): lowered to
//     llvm.mlir.constant (if mem2reg promotes the alloca)
//   - arith.constant (NumberDecl): lowered to llvm.mlir.constant
//
// Dispatch strategy:
//   llvm.alloca with NameLoc          -> DW_TAG_variable + dbg.declare
//   other op with NameLoc, no alloca  -> DW_TAG_variable + dbg.value
//
// When an llvm.alloca with NameLoc("x") exists in a function, scalar ops with
// the same NameLoc("x") are suppressed to avoid duplicate debug entries.
//
//===----------------------------------------------------------------------===//

#include "ada/Passes.h"
#include "mlir/Dialect/LLVMIR/LLVMAttrs.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/Pass/Pass.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/BinaryFormat/Dwarf.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/Path.h"

using namespace mlir;

namespace {

static LLVM::DIBasicTypeAttr makeDIIntType(MLIRContext *ctx,
                                           IntegerType intType) {
  unsigned w = intType.getWidth();
  unsigned enc =
      (w == 1) ? llvm::dwarf::DW_ATE_boolean : llvm::dwarf::DW_ATE_signed;
  unsigned sizeInBits = llvm::alignTo(w, 8);
  return LLVM::DIBasicTypeAttr::get(ctx, llvm::dwarf::DW_TAG_base_type,
                                    ("integer_" + llvm::Twine(w)).str(),
                                    sizeInBits, enc);
}

struct FinalizeAdaObjectPass
    : public PassWrapper<FinalizeAdaObjectPass, OperationPass<ModuleOp>> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(FinalizeAdaObjectPass)

  void runOnOperation() final {
    MLIRContext *ctx = &getContext();
    ModuleOp module = getOperation();

    // Pass 1: record (func, name) pairs for all llvm.alloca ops with NameLoc.
    // Used to suppress scalar debug entries when an alloca already tracks the
    // variable with dbg.declare.
    using FuncNamePair = std::pair<Operation *, StringAttr>;
    llvm::DenseSet<FuncNamePair> allocaNames;
    module.walk([&](LLVM::AllocaOp op) {
      auto nl = dyn_cast<NameLoc>(op.getLoc());
      if (!nl)
        return;
      auto func = op->getParentOfType<LLVM::LLVMFuncOp>();
      if (func)
        allocaNames.insert({func, nl.getName()});
    });

    // Pass 2: collect debug entries.
    struct DebugEntry {
      Operation *op;
      Operation *func;
      StringAttr name;
      Location innerLoc;
      bool isDeclare; // alloca: dbg.declare; scalar: dbg.value
    };
    llvm::SmallVector<DebugEntry> entries;

    module.walk([&](Operation *op) {
      auto nl = dyn_cast<NameLoc>(op->getLoc());
      if (!nl)
        return;

      auto func = op->getParentOfType<LLVM::LLVMFuncOp>();
      if (!func)
        return;

      bool isAlloca = isa<LLVM::AllocaOp>(op);
      if (!isAlloca) {
        // Skip scalar ops when an alloca with the same name exists.
        if (allocaNames.contains({func, nl.getName()}))
          return;
        // Must produce exactly one result to emit dbg.value.
        if (op->getNumResults() != 1)
          return;
      }

      entries.push_back({op, func, nl.getName(), nl.getChildLoc(), isAlloca});
    });

    // Pass 3: emit debug intrinsics.
    for (auto &entry : entries) {
      auto fusedLoc = dyn_cast<FusedLoc>(entry.func->getLoc());
      if (!fusedLoc)
        continue;
      auto subprogram =
          dyn_cast_or_null<LLVM::DISubprogramAttr>(fusedLoc.getMetadata());
      if (!subprogram)
        continue;

      LLVM::DIFileAttr fileAttr;
      unsigned line = 0;
      if (auto flc = dyn_cast<FileLineColRange>(entry.innerLoc)) {
        StringRef filePath = flc.getFilename().getValue();
        fileAttr =
            LLVM::DIFileAttr::get(ctx, llvm::sys::path::filename(filePath),
                                  llvm::sys::path::parent_path(filePath));
        line = flc.getStartLine();
      } else {
        fileAttr = subprogram.getFile();
      }

      mlir::Type elemType = entry.isDeclare
                                ? cast<LLVM::AllocaOp>(entry.op).getElemType()
                                : entry.op->getResult(0).getType();
      auto intType = dyn_cast<IntegerType>(elemType);
      if (!intType)
        continue; // silently skip floats and other types for now

      auto diType = makeDIIntType(ctx, intType);
      auto varInfo = LLVM::DILocalVariableAttr::get(
          subprogram, entry.name, fileAttr, line,
          /*arg=*/0, /*alignInBits=*/0, diType, LLVM::DIFlags::Zero);

      OpBuilder builder(entry.op);
      builder.setInsertionPointAfter(entry.op);
      if (entry.isDeclare)
        LLVM::DbgDeclareOp::create(builder, entry.op->getLoc(),
                                   cast<LLVM::AllocaOp>(entry.op).getRes(),
                                   varInfo);
      else
        LLVM::DbgValueOp::create(builder, entry.op->getLoc(),
                                 entry.op->getResult(0), varInfo);
    }
  }
};
} // namespace

std::unique_ptr<mlir::Pass> mlir::ada::createFinalizeAdaObjectPass() {
  return std::make_unique<FinalizeAdaObjectPass>();
}
