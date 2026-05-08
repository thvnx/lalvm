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

#include "mlir/Dialect/LLVMIR/LLVMAttrs.h"
#include "mlir/Dialect/LLVMIR/LLVMTypes.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/Support/LLVM.h"
#include "mlir/Support/TypeID.h"
#include "ada/Dialect.h"
#include "ada/Passes.h"

#include "mlir/Conversion/ArithToLLVM/ArithToLLVM.h"
#include "mlir/Conversion/ControlFlowToLLVM/ControlFlowToLLVM.h"
#include "mlir/Conversion/FuncToLLVM/ConvertFuncToLLVM.h"
#include "mlir/Conversion/FuncToLLVM/ConvertFuncToLLVMPass.h"
#include "mlir/Conversion/LLVMCommon/ConversionTarget.h"
#include "mlir/Conversion/LLVMCommon/TypeConverter.h"
#include "mlir/Conversion/SCFToControlFlow/SCFToControlFlow.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Transforms/DialectConversion.h"
#include "llvm/Support/Casting.h"
#include <memory>
#include <utility>

using namespace mlir;

//===----------------------------------------------------------------------===//
// AdaToLLVMLoweringPass
//===----------------------------------------------------------------------===//

namespace {
struct AdaToLLVMLoweringPass
    : public PassWrapper<AdaToLLVMLoweringPass, OperationPass<ModuleOp>> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(AdaToLLVMLoweringPass)

  void getDependentDialects(DialectRegistry &registry) const override {
    registry.insert<LLVM::LLVMDialect, func::FuncDialect, arith::ArithDialect>();
  }
  void runOnOperation() final;
};
} // namespace

struct ReturnOpLowering : public OpRewritePattern<ada::ReturnOp> {
  using OpRewritePattern<ada::ReturnOp>::OpRewritePattern;

  LogicalResult matchAndRewrite(ada::ReturnOp op,
                                PatternRewriter &rewriter) const final {
    // During this lowering, we expect that all function calls have been
    // inlined.
    //if (op.hasOperand())
    //  return failure();

    // We lower "ada.return" directly to "func.return".
    rewriter.replaceOpWithNewOp<func::ReturnOp>(op, op.getOperands());
    return success();
  }
};

//===----------------------------------------------------------------------===//
// AdaToLLVM RewritePatterns: Binary operations
//===----------------------------------------------------------------------===//

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
      return failure();
    return success();
  }
};
using AddOpLowering = NumericBinaryOpLowering<ada::AddOp, arith::AddIOp, arith::AddFOp>;
using SubOpLowering = NumericBinaryOpLowering<ada::SubOp, arith::SubIOp, arith::SubFOp>;
using MulOpLowering = NumericBinaryOpLowering<ada::MulOp, arith::MulIOp, arith::MulFOp>;

//===----------------------------------------------------------------------===//
// AdaToLLVM RewritePatterns: Func operations
//===----------------------------------------------------------------------===//

struct FuncOpLowering : public OpConversionPattern<ada::FuncOp> {
  using OpConversionPattern<ada::FuncOp>::OpConversionPattern;

  LogicalResult
  matchAndRewrite(ada::FuncOp op, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const final {
    // // We only lower the main function as we expect that all other functions
    // // have been inlined.
    // if (op.getName() != "main")
    //   return failure();

    // // Verify that the given main has no inputs and results.
    // if (op.getNumArguments() || op.getFunctionType().getNumResults()) {
    //   return rewriter.notifyMatchFailure(op, [](Diagnostic &diag) {
    //     diag << "expected 'main' to have 0 inputs and 0 results";
    //   });
    // }

    // Create a new func.func function, with the same region.
    auto func = rewriter.create<mlir::func::FuncOp>(op.getLoc(), op.getName(),
                                                    op.getFunctionType());
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
    rewriter.inlineRegionBefore(op.getRegion(), func.getBody(), func.end());
    rewriter.eraseOp(op);
    return success();
  }
};

void AdaToLLVMLoweringPass::runOnOperation() {
  // The first thing to define is the conversion target. This will define the
  // final target for this lowering. For this lowering, we are only targeting
  // the LLVM dialect.
  LLVMConversionTarget target(getContext());


  // We define the specific operations, or dialects, that are legal targets for
  // this lowering. In our case, we are lowering to a combination of the
  // `Affine`, `Arith`, `Func`, and `MemRef` dialects.
  //target.addLegalDialect<arith::ArithDialect, func::FuncDialect>();

  target.addLegalOp<ModuleOp>();

  // During this lowering, we will also be lowering the MemRef types, that are
  // currently being operated on, to a representation in LLVM. To perform this
  // conversion we use a TypeConverter as part of the lowering. This converter
  // details how one type maps to another. This is necessary now that we will be
  // doing more complicated lowerings, involving loop region arguments.
  LLVMTypeConverter typeConverter(&getContext());

  // Provide the patterns used for lowering.
  RewritePatternSet patterns(&getContext());
  populateSCFToControlFlowConversionPatterns(patterns);
  mlir::arith::populateArithToLLVMConversionPatterns(typeConverter, patterns);
  cf::populateControlFlowToLLVMConversionPatterns(typeConverter, patterns);
  populateFuncToLLVMConversionPatterns(typeConverter, patterns);

  patterns.add<ReturnOpLowering, FuncOpLowering, ProcOpLowering,
               AddOpLowering, SubOpLowering, MulOpLowering>(&getContext());

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
