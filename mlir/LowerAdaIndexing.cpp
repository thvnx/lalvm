// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

#include "ada/Dialect.h"
#include "ada/Passes.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Transforms/DialectConversion.h"
#include <algorithm>

using namespace mlir;

namespace {

// Bounds of the currently supported static array objects.
static ada::ArrayTypeInfoAttr staticArrayInfo(ada::IndexOp op) {
  auto qual = ada::objectValueType(op.getArray().getType());
  auto type = qual ? ada::rootOfAdaType(op, qual.getAdaType()) : ada::TypeOp{};
  auto info =
      type ? dyn_cast_or_null<ada::ArrayTypeInfoAttr>(type.getTypeInfoAttr())
           : ada::ArrayTypeInfoAttr{};
  return info && info.staticLower(0) && info.staticUpper(0)
             ? info
             : ada::ArrayTypeInfoAttr{};
}

struct IndexOpLowering : OpRewritePattern<ada::IndexOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(ada::IndexOp op,
                                PatternRewriter &rewriter) const override {
    if (op.getOffset())
      return failure();
    auto info = staticArrayInfo(op);
    if (!info)
      return rewriter.notifyMatchFailure(op, "missing static array bounds");
    Location loc = op.getLoc();
    Value index = op.getIndex();
    auto indexQual = cast<ada::QualType>(index.getType());
    auto lower = ada::boundValue(info.staticLower(0));
    auto upper = ada::boundValue(info.staticUpper(0));
    bool checked = false;
    if (auto check = index.getDefiningOp<ada::IndexCheckOp>())
      if (auto range = check.getRange().getDefiningOp<ada::RangeOp>()) {
        auto [lo, hi] = range.staticBounds();
        auto loAttr = dyn_cast_or_null<IntegerAttr>(lo);
        auto hiAttr = dyn_cast_or_null<IntegerAttr>(hi);
        checked =
            loAttr && hiAttr &&
            llvm::APSInt::compareValues(ada::boundValue(loAttr), lower) == 0 &&
            llvm::APSInt::compareValues(ada::boundValue(hiAttr), upper) == 0;
      }
    if (!checked) {
      if (auto constant = index.getDefiningOp<ada::ConstantOp>()) {
        if (auto value = constant.getIntValue())
          checked = ada::inStaticRange(*value, lower, upper);
      } else if (auto typeInfo =
                     ada::intTypeInfoOfAdaType(op, indexQual.getAdaType())) {
        if (auto bounds = typeInfo.staticBounds())
          checked = ada::inStaticRange(bounds->first, lower, upper) &&
                    ada::inStaticRange(bounds->second, lower, upper);
      }
    }
    if (!checked) {
      auto indexSym = info.getIndexTypes()[0];
      auto base = ada::baseOfAdaType(op, indexSym);
      if (!base)
        return rewriter.notifyMatchFailure(op, "unresolved index base type");
      auto subtype = ada::lookupAdaType(op, indexSym);
      auto machine = subtype.getMlirType();
      auto baseQual = ada::QualType::get(getContext(), machine, base);
      auto checkedQual = ada::QualType::get(getContext(), machine, indexSym);
      Value range = ada::getOrCreateStaticRange(rewriter, loc, indexSym,
                                                baseQual, lower, upper);
      index =
          ada::IndexCheckOp::create(rewriter, loc, checkedQual, index, range);
    }

    auto machine = cast<IntegerType>(indexQual.getMlirType());
    Value raw = ada::UnwrapOp::create(rewriter, loc, machine, index);
    auto wideType =
        rewriter.getIntegerType(std::max(64u, machine.getWidth() + 1));
    bool unsignedIndex = ada::isUnsignedOrderQualType(op, indexQual);
    if (unsignedIndex)
      raw = arith::ExtUIOp::create(rewriter, loc, wideType, raw);
    else
      raw = arith::ExtSIOp::create(rewriter, loc, wideType, raw);
    Value first = arith::ConstantOp::create(
        rewriter, loc,
        rewriter.getIntegerAttr(wideType,
                                lower.extOrTrunc(wideType.getWidth())));
    Value offset = arith::SubIOp::create(rewriter, loc, raw, first);
    rewriter.modifyOpInPlace(op, [&] {
      op.getIndexMutable().assign(index);
      op.getOffsetMutable().assign(offset);
    });
    return success();
  }
};

struct LowerAdaIndexingPass
    : PassWrapper<LowerAdaIndexingPass, OperationPass<ModuleOp>> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(LowerAdaIndexingPass)

  void getDependentDialects(DialectRegistry &registry) const override {
    registry.insert<ada::AdaDialect, arith::ArithDialect>();
  }
  StringRef getArgument() const final { return "ada-lower-indexing"; }
  StringRef getName() const final { return "LowerAdaIndexing"; }
  StringRef getDescription() const final {
    return "Check Ada indices and normalize storage offsets";
  }
  void runOnOperation() final {
    ConversionTarget target(getContext());
    target.markUnknownOpDynamicallyLegal([](Operation *) { return true; });
    target.addDynamicallyLegalOp<ada::IndexOp>(
        [](ada::IndexOp op) { return bool(op.getOffset()); });
    RewritePatternSet patterns(&getContext());
    patterns.add<IndexOpLowering>(&getContext());
    if (failed(applyPartialConversion(getOperation(), target,
                                      std::move(patterns))))
      signalPassFailure();
  }
};

} // namespace

std::unique_ptr<Pass> mlir::ada::createLowerAdaIndexingPass() {
  return std::make_unique<LowerAdaIndexingPass>();
}
