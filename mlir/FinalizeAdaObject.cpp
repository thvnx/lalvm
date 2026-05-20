//===- FinalizeAdaObject.cpp - Finalize ada.object ops --------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass finalizes ada.object ops by emitting LLVM debug intrinsics and
// erasing the ops. It runs after DIScopeForLLVMFuncOpPass so that
// DISubprogramAttr is available on each llvm.func.
//
// ada.object ops are kept alive through LowerToLLVM so that the $object
// operand is remapped by the conversion framework (memref<T> becomes llvm.ptr).
// This pass is the consumer of those surviving ops.
//
// Dispatch strategy:
//   llvm.alloca result  -> DW_TAG_variable + llvm.intr.dbg.declare
//   scalar value        -> DW_TAG_variable + llvm.intr.dbg.value
// Block-argument cases (parameters, i.e. block arguments) are not yet handled.
//
//===----------------------------------------------------------------------===//

#include "ada/Dialect.h"
#include "ada/Passes.h"
#include "mlir/Dialect/LLVMIR/LLVMAttrs.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/Pass/Pass.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/BinaryFormat/Dwarf.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/Path.h"

using namespace mlir;

namespace {

// Returns a DIBasicTypeAttr for an MLIR IntegerType. Type name is a
// placeholder; proper Ada type names will be threaded via ada.type in the
// future.
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
    llvm::SmallVector<ada::ObjectOp> toErase;

    getOperation().walk([&](ada::ObjectOp op) {
      toErase.push_back(op);

      auto funcOp = op->getParentOfType<LLVM::LLVMFuncOp>();
      if (!funcOp) {
        // TODO: library-level objects live outside any function; emit
        // DIGlobalVariableAttr on the llvm.mlir.global op instead.
        op.emitWarning("ada.object outside any llvm.func: debug info skipped");
        return;
      }
      auto fusedLoc = dyn_cast<FusedLoc>(funcOp.getLoc());
      if (!fusedLoc) {
        op.emitWarning("enclosing llvm.func has no FusedLoc: "
                       "debug info skipped");
        return;
      }
      auto subprogram =
          dyn_cast_or_null<LLVM::DISubprogramAttr>(fusedLoc.getMetadata());
      if (!subprogram) {
        op.emitWarning("enclosing llvm.func FusedLoc carries no "
                       "DISubprogramAttr: debug info skipped");
        return;
      }

      // File and source line for the variable declaration.
      LLVM::DIFileAttr fileAttr;
      unsigned line = 0;
      if (auto flc = dyn_cast<FileLineColRange>(op.getLoc())) {
        StringRef filePath = flc.getFilename().getValue();
        fileAttr =
            LLVM::DIFileAttr::get(ctx, llvm::sys::path::filename(filePath),
                                  llvm::sys::path::parent_path(filePath));
        line = flc.getStartLine();
      } else {
        fileAttr = subprogram.getFile();
      }

      // Determine the declared type and DWARF intrinsic from $object.
      mlir::Value object = op.getObject();
      auto allocaOp = object.getDefiningOp<LLVM::AllocaOp>();

      mlir::Type elemType =
          allocaOp ? allocaOp.getElemType() : object.getType();
      auto intType = dyn_cast<IntegerType>(elemType);
      if (!intType) {
        op.emitWarning("unsupported object type for debug info: ") << elemType;
        return;
      }

      auto diType = makeDIIntType(ctx, intType);
      auto varInfo = LLVM::DILocalVariableAttr::get(
          subprogram, op.getName(), fileAttr, line,
          /*arg=*/0, /*alignInBits=*/0, diType, LLVM::DIFlags::Zero);

      OpBuilder builder(op);
      if (allocaOp) {
        builder.setInsertionPointAfter(allocaOp);
        LLVM::DbgDeclareOp::create(builder, allocaOp.getLoc(), object, varInfo);
      } else {
        LLVM::DbgValueOp::create(builder, op.getLoc(), object, varInfo);
      }
    });

    for (ada::ObjectOp op : toErase)
      op.erase();
  }
};
} // namespace

std::unique_ptr<mlir::Pass> mlir::ada::createFinalizeAdaObjectPass() {
  return std::make_unique<FinalizeAdaObjectPass>();
}
