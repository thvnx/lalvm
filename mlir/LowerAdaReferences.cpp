// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

#include "ada/Dialect.h"
#include "ada/Passes.h"
#include "mlir/Dialect/ControlFlow/IR/ControlFlow.h"
#include "mlir/Dialect/ControlFlow/Transforms/StructuralTypeConversions.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/Dialect/SCF/Transforms/Patterns.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Transforms/DialectConversion.h"

using namespace mlir;

namespace {

//===----------------------------------------------------------------------===//
// ObjectOpLowering
//===----------------------------------------------------------------------===//

struct ObjectOpLowering : OpConversionPattern<ada::ObjectOp> {
  using OpConversionPattern::OpConversionPattern;

  LogicalResult
  matchAndRewrite(ada::ObjectOp op, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override {
    auto slotType = dyn_cast_or_null<MemRefType>(
        getTypeConverter()->convertType(op.getType()));
    if (!slotType)
      return rewriter.notifyMatchFailure(op, "unresolved object base type");
    auto qual =
        cast<ada::QualType>(cast<ada::RefType>(op.getType()).getValueType());
    Location loc = ada::attachAdaTypeRef(op.getLoc(), qual.getAdaType());
    Value slot = ada::AllocaOp::create(rewriter, loc, slotType);
    if (Value initial = adaptor.getInitial()) {
      initial = ada::requalify(rewriter, ada::unnamedLoc(op.getLoc()), initial,
                               slotType.getElementType());
      memref::StoreOp::create(rewriter, ada::unnamedLoc(op.getLoc()), initial,
                              slot);
    }
    rewriter.replaceOp(op, slot);
    return success();
  }
};

//===----------------------------------------------------------------------===//
// ReadOpLowering
//===----------------------------------------------------------------------===//

struct ReadOpLowering : OpConversionPattern<ada::ReadOp> {
  using OpConversionPattern::OpConversionPattern;

  LogicalResult
  matchAndRewrite(ada::ReadOp op, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override {
    Value value =
        memref::LoadOp::create(rewriter, op.getLoc(), adaptor.getReference());
    rewriter.replaceOp(op,
                       ada::requalify(rewriter, ada::unnamedLoc(op.getLoc()),
                                      value, op.getType()));
    return success();
  }
};

//===----------------------------------------------------------------------===//
// WriteOpLowering
//===----------------------------------------------------------------------===//

struct WriteOpLowering : OpConversionPattern<ada::WriteOp> {
  using OpConversionPattern::OpConversionPattern;

  LogicalResult
  matchAndRewrite(ada::WriteOp op, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override {
    auto slotType = cast<MemRefType>(adaptor.getReference().getType());
    Value value = ada::requalify(rewriter, ada::unnamedLoc(op.getLoc()),
                                 adaptor.getValue(), slotType.getElementType());
    rewriter.replaceOpWithNewOp<memref::StoreOp>(op, value,
                                                 adaptor.getReference());
    return success();
  }
};

//===----------------------------------------------------------------------===//
// IndexOpLowering
//===----------------------------------------------------------------------===//

struct IndexOpLowering : OpConversionPattern<ada::IndexOp> {
  using OpConversionPattern::OpConversionPattern;

  LogicalResult
  matchAndRewrite(ada::IndexOp op, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override {
    Type type = getTypeConverter()->convertType(op.getType());
    if (!type)
      return rewriter.notifyMatchFailure(op, "unresolved component base type");
    rewriter.replaceOpWithNewOp<ada::IndexOp>(
        op, type, adaptor.getArray(), adaptor.getIndex(), adaptor.getOffset());
    return success();
  }
};

//===----------------------------------------------------------------------===//
// FunctionOpLowering
//===----------------------------------------------------------------------===//

struct FunctionOpLowering : OpConversionPattern<func::FuncOp> {
  using OpConversionPattern::OpConversionPattern;

  LogicalResult
  matchAndRewrite(func::FuncOp op, OpAdaptor,
                  ConversionPatternRewriter &rewriter) const override {
    const TypeConverter &converter = *this->getTypeConverter();
    auto type = op.getFunctionType();
    TypeConverter::SignatureConversion signature(type.getNumInputs());
    SmallVector<Type> results;
    if (failed(converter.convertSignatureArgs(type.getInputs(), signature)) ||
        failed(converter.convertTypes(type.getResults(), results)))
      return failure();
    if (failed(
            rewriter.convertRegionTypes(&op.getBody(), converter, &signature)))
      return failure();
    rewriter.modifyOpInPlace(op, [&] {
      op.setType(FunctionType::get(this->getContext(),
                                   signature.getConvertedTypes(), results));
    });
    return success();
  }
};

//===----------------------------------------------------------------------===//
// CallOpLowering
//===----------------------------------------------------------------------===//

struct CallOpLowering : OpConversionPattern<func::CallOp> {
  using OpConversionPattern::OpConversionPattern;

  LogicalResult
  matchAndRewrite(func::CallOp op, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override {
    rewriter.modifyOpInPlace(op,
                             [&] { op->setOperands(adaptor.getOperands()); });
    return success();
  }
};

//===----------------------------------------------------------------------===//
// LowerAdaReferencesPass
//===----------------------------------------------------------------------===//

struct LowerAdaReferencesPass
    : PassWrapper<LowerAdaReferencesPass, OperationPass<ModuleOp>> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(LowerAdaReferencesPass)

  void getDependentDialects(DialectRegistry &registry) const override {
    registry.insert<ada::AdaDialect, func::FuncDialect, memref::MemRefDialect,
                    cf::ControlFlowDialect, scf::SCFDialect>();
  }
  void runOnOperation() final;
  StringRef getName() const final { return "LowerAdaReferences"; }
  StringRef getArgument() const final { return "ada-lower-references"; }
  StringRef getDescription() const final {
    return "Lower Ada object references and accesses to storage";
  }
};

} // namespace

void LowerAdaReferencesPass::runOnOperation() {
  ModuleOp module = getOperation();
  TypeConverter converter;
  converter.addConversion([](Type type) { return type; });
  converter.addConversion([&](ada::RefType type) -> Type {
    auto qual = cast<ada::QualType>(type.getValueType());
    auto element = ada::baseOfAdaType(module, qual);
    if (!element)
      return {};
    return MemRefType::get({}, element);
  });

  // Keep nominal types on named block arguments.
  module.walk([&](Operation *op) {
    for (Region &region : op->getRegions())
      for (Block &block : region)
        for (BlockArgument arg : block.getArguments()) {
          auto qual = dyn_cast<ada::QualType>(arg.getType());
          if (auto ref = dyn_cast<ada::RefType>(arg.getType()))
            qual = cast<ada::QualType>(ref.getValueType());
          if (qual &&
              !arg.getLoc()->findInstanceOf<FusedLocWith<ada::DITypeRefAttr>>())
            arg.setLoc(ada::attachAdaTypeRef(arg.getLoc(), qual.getAdaType()));
        }
  });

  ConversionTarget target(getContext());
  target.markUnknownOpDynamicallyLegal([&](Operation *op) {
    return converter.isLegal(op) &&
           llvm::all_of(op->getRegions(), [&](Region &region) {
             return converter.isLegal(&region);
           });
  });
  target.addIllegalOp<ada::ObjectOp, ada::ReadOp, ada::WriteOp, ada::SubpOp,
                      ada::CallOp, ada::ReturnOp>();
  target.addDynamicallyLegalOp<func::FuncOp>([&](func::FuncOp op) {
    return converter.isSignatureLegal(op.getFunctionType()) &&
           converter.isLegal(&op.getBody());
  });

  RewritePatternSet patterns(&getContext());
  patterns.add<ObjectOpLowering, ReadOpLowering, WriteOpLowering,
               IndexOpLowering, FunctionOpLowering, CallOpLowering>(
      converter, &getContext());
  cf::populateCFStructuralTypeConversionsAndLegality(converter, patterns,
                                                     target);
  scf::populateSCFStructuralTypeConversionsAndLegality(converter, patterns,
                                                       target);
  if (failed(applyPartialConversion(module, target, std::move(patterns))))
    signalPassFailure();
}

std::unique_ptr<Pass> mlir::ada::createLowerAdaReferencesPass() {
  return std::make_unique<LowerAdaReferencesPass>();
}
