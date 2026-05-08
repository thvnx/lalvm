//===- LowerToLLVM.cpp - Lowering from Ada to LLVM ------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements lowering of Ada dialect operations to the LLVM dialect.
//
//===----------------------------------------------------------------------===//

#include "ada/Dialect.h"
#include "ada/Passes.h"
#include "mlir/Dialect/LLVMIR/LLVMAttrs.h"
#include "mlir/Dialect/LLVMIR/LLVMTypes.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/Support/LLVM.h"
#include "mlir/Support/TypeID.h"

#include "mlir/Conversion/ArithToLLVM/ArithToLLVM.h"
#include "mlir/Conversion/ControlFlowToLLVM/ControlFlowToLLVM.h"
#include "mlir/Conversion/FuncToLLVM/ConvertFuncToLLVM.h"
#include "mlir/Conversion/FuncToLLVM/ConvertFuncToLLVMPass.h"
#include "mlir/Conversion/LLVMCommon/ConversionTarget.h"
#include "mlir/Conversion/LLVMCommon/TypeConverter.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Transforms/DialectConversion.h"
#include "llvm/Support/Casting.h"
#include <memory>
#include <utility>

using namespace mlir;

// This file implements a single lowering pass that converts the Ada dialect
// (plus the Arith and Func dialects it relies on) directly to the LLVM dialect.
// The pass uses FullConversion, meaning every op must be lowered — no Ada ops
// are allowed to survive.
//
// Lowering chain overview:
//   ada.func / ada.proc  →  func.func
//   ada.return           →  func.return
//   ada.add/sub/mul      →  arith.addi/subi/muli  (integers)
//                           arith.addf/subf/mulf  (floats)
//   func.func / arith.*  →  LLVM dialect  (via upstream conversion patterns)

//===----------------------------------------------------------------------===//
// AdaToLLVMLoweringPass
//===----------------------------------------------------------------------===//

namespace {
struct AdaToLLVMLoweringPass
    : public PassWrapper<AdaToLLVMLoweringPass, OperationPass<ModuleOp>> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(AdaToLLVMLoweringPass)

  void getDependentDialects(DialectRegistry &registry) const override {
    registry
        .insert<LLVM::LLVMDialect, func::FuncDialect, arith::ArithDialect>();
  }
  void runOnOperation() final;
};
} // namespace

struct ReturnOpLowering : public OpRewritePattern<ada::ReturnOp> {
  using OpRewritePattern<ada::ReturnOp>::OpRewritePattern;

  LogicalResult matchAndRewrite(ada::ReturnOp op,
                                PatternRewriter &rewriter) const final {
    // We lower "ada.return" directly to "func.return".
    rewriter.replaceOpWithNewOp<func::ReturnOp>(op, op.getOperands());
    return success();
  }
};

//===----------------------------------------------------------------------===//
// AdaToLLVM RewritePatterns: Binary operations
//===----------------------------------------------------------------------===//

// Generic lowering for binary arithmetic ops. The Ada dialect uses a single
// op per operation (add/sub/mul) that is type-agnostic; the arith dialect
// has separate integer and float variants, so we dispatch on the operand type.
template <typename AdaOp, typename IntOp, typename FloatOp>
struct NumericBinaryOpLowering : public OpRewritePattern<AdaOp> {
  using OpRewritePattern<AdaOp>::OpRewritePattern;

  LogicalResult matchAndRewrite(AdaOp op,
                                PatternRewriter &rewriter) const final {
    Type type = op->getOperand(0).getType();
    if (mlir::isa<mlir::IntegerType>(type))
      rewriter.replaceOpWithNewOp<IntOp>(op, op->getOperands());
    else if (mlir::isa<mlir::FloatType>(type))
      rewriter.replaceOpWithNewOp<FloatOp>(op, op->getOperands());
    else
      return rewriter.notifyMatchFailure(op, [type](Diagnostic &diag) {
        diag << "unsupported operand type: " << type;
      });
    return success();
  }
};
using AddOpLowering =
    NumericBinaryOpLowering<ada::AddOp, arith::AddIOp, arith::AddFOp>;
using SubOpLowering =
    NumericBinaryOpLowering<ada::SubOp, arith::SubIOp, arith::SubFOp>;
using MulOpLowering =
    NumericBinaryOpLowering<ada::MulOp, arith::MulIOp, arith::MulFOp>;

//===----------------------------------------------------------------------===//
// AdaToLLVM RewritePatterns: Func operations
//===----------------------------------------------------------------------===//

// ada.func and ada.proc are isomorphic to func.func at this level; we just
// swap the op type and move the region over. The upstream FuncToLLVM pass then
// handles the func.func → llvm.func conversion.
struct FuncOpLowering : public OpConversionPattern<ada::FuncOp> {
  using OpConversionPattern<ada::FuncOp>::OpConversionPattern;

  LogicalResult
  matchAndRewrite(ada::FuncOp op, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const final {
    // Create a new func.func function, with the same region.
    auto func = rewriter.create<mlir::func::FuncOp>(op.getLoc(), op.getName(),
                                                    op.getFunctionType());
    if (ArrayAttr argAttrs = op.getArgAttrsAttr())
      func.setAllArgAttrs(argAttrs);
    if (ArrayAttr resAttrs = op.getResAttrsAttr())
      func.setAllResultAttrs(resAttrs);
    rewriter.inlineRegionBefore(op.getRegion(), func.getBody(), func.end());
    rewriter.eraseOp(op);
    return success();
  }
};

struct ProcOpLowering : public OpConversionPattern<ada::ProcOp> {
  using OpConversionPattern<ada::ProcOp>::OpConversionPattern;

  LogicalResult
  matchAndRewrite(ada::ProcOp op, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const final {
    auto func = rewriter.create<mlir::func::FuncOp>(op.getLoc(), op.getName(),
                                                    op.getFunctionType());
    if (ArrayAttr argAttrs = op.getArgAttrsAttr())
      func.setAllArgAttrs(argAttrs);
    rewriter.inlineRegionBefore(op.getRegion(), func.getBody(), func.end());
    rewriter.eraseOp(op);
    return success();
  }
};

void AdaToLLVMLoweringPass::runOnOperation() {
  // The first thing to define is the conversion target. This will define the
  // final target for this lowering. For this lowering, we are only targeting
  // the LLVM dialect.
  // LLVMConversionTarget marks all LLVM dialect ops as legal and everything
  // else (including ada.*) as illegal, driving the full conversion.
  LLVMConversionTarget target(getContext());
  target.addLegalOp<ModuleOp>();

  // LLVMTypeConverter maps MLIR types (i32, f64, …) to their LLVM equivalents.
  // It is threaded through the upstream conversion patterns that need it.
  LLVMTypeConverter typeConverter(&getContext());

  // Provide the patterns used for lowering.
  RewritePatternSet patterns(&getContext());
  // TODO: add populateSCFToControlFlowConversionPatterns once the Ada codegen
  // emits SCF ops (e.g. for if/loop statements).
  mlir::arith::populateArithToLLVMConversionPatterns(typeConverter, patterns);
  cf::populateControlFlowToLLVMConversionPatterns(typeConverter, patterns);
  populateFuncToLLVMConversionPatterns(typeConverter, patterns);

  patterns.add<ReturnOpLowering, FuncOpLowering, ProcOpLowering, AddOpLowering,
               SubOpLowering, MulOpLowering>(&getContext());

  // We want to completely lower to LLVM, so we use a `FullConversion`. This
  // ensures that only legal operations will remain after the conversion.
  auto module = getOperation();
  if (failed(applyFullConversion(module, target, std::move(patterns))))
    signalPassFailure();
}

/// Create a pass for lowering Ada dialect operations to the LLVM dialect.
std::unique_ptr<mlir::Pass> mlir::ada::createLowerToLLVMPass() {
  return std::make_unique<AdaToLLVMLoweringPass>();
}
