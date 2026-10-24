// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

#include "ada/Dialect.h"
#include "ada/Passes.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Support/TypeID.h"
#include "mlir/Transforms/DialectConversion.h"
#include "llvm/ADT/APSInt.h"

using namespace mlir;

/// Whether `knownLo..knownHi` lies within `lo..hi`.
static bool rangeWithin(const APSInt &knownLo, const APSInt &knownHi,
                        const APSInt &lo, const APSInt &hi) {
  return ada::inStaticRange(knownLo, lo, hi) &&
         ada::inStaticRange(knownHi, lo, hi);
}

/// Whether `value` is known to lie in `lo..hi`: a constant within it, or a
/// value whose type's static range is within it.
static bool knownWithin(Operation *from, Value value, const APSInt &lo,
                        const APSInt &hi) {
  if (auto constant = value.getDefiningOp<ada::ConstantOp>())
    if (std::optional<APSInt> known = constant.getIntValue())
      return ada::inStaticRange(*known, lo, hi);
  ada::IntegerTypeInfoAttr info = ada::intTypeInfoOfAdaType(
      from, cast<ada::QualType>(value.getType()).getAdaType());
  IntegerAttr knownLo = info ? info.staticLower() : IntegerAttr();
  IntegerAttr knownHi = info ? info.staticUpper() : IntegerAttr();
  return knownLo && knownHi &&
         rangeWithin(ada::boundValue(knownLo), ada::boundValue(knownHi), lo,
                     hi);
}

/// `lo..hi` restricted to the values a `type` integer represents (unsigned when
/// `typeUnsigned`), with `type`'s width. A range no value lies in when the
/// restriction is empty.
static std::pair<APInt, APInt> clampTo(const APSInt &lo, const APSInt &hi,
                                       IntegerType type, bool typeUnsigned) {
  unsigned bits = type.getWidth();
  APSInt min(typeUnsigned ? APInt::getMinValue(bits)
                          : APInt::getSignedMinValue(bits),
             typeUnsigned);
  APSInt max(typeUnsigned ? APInt::getMaxValue(bits)
                          : APInt::getSignedMaxValue(bits),
             typeUnsigned);
  const APSInt &l = APSInt::compareValues(lo, min) < 0 ? min : lo;
  const APSInt &h = APSInt::compareValues(hi, max) > 0 ? max : hi;
  if (APSInt::compareValues(l, h) > 0)
    return {max, min};
  return {l.extOrTrunc(bits), h.extOrTrunc(bits)};
}

/// Give `value` the machine representation of `targetQual`: sign or zero extend
/// an integer as the source reads its values, truncate it, or change a float's
/// precision. The machine value is kept as is between the same types.
static Value changeRepresentation(PatternRewriter &rewriter, Location loc,
                                  Value value, ada::QualType targetQual,
                                  bool sourceUnsigned) {
  Type sourceType = cast<ada::QualType>(value.getType()).getMlirType();
  Type targetType = targetQual.getMlirType();
  Value raw = ada::UnwrapOp::create(rewriter, loc, sourceType, value);
  unsigned sourceWidth = sourceType.getIntOrFloatBitWidth();
  unsigned targetWidth = targetType.getIntOrFloatBitWidth();
  if (isa<FloatType>(sourceType)) {
    if (targetWidth < sourceWidth)
      raw = arith::TruncFOp::create(rewriter, loc, targetType, raw);
    else if (targetWidth > sourceWidth)
      raw = arith::ExtFOp::create(rewriter, loc, targetType, raw);
  } else if (targetWidth < sourceWidth) {
    raw = arith::TruncIOp::create(rewriter, loc, targetType, raw);
  } else if (targetWidth > sourceWidth) {
    if (sourceUnsigned)
      raw = arith::ExtUIOp::create(rewriter, loc, targetType, raw);
    else
      raw = arith::ExtSIOp::create(rewriter, loc, targetType, raw);
  }
  return ada::WrapOp::create(rewriter, loc, targetQual, raw);
}

namespace {

/// Lower a value conversion that keeps the machine representation: a range
/// check into a constrained target (it establishes the target subtype), or a
/// requalification when no check is needed (@rm{4-6}).
struct SameRepresentationConversion : public OpRewritePattern<ada::ConvertOp> {
  using OpRewritePattern<ada::ConvertOp>::OpRewritePattern;

  LogicalResult matchAndRewrite(ada::ConvertOp op,
                                PatternRewriter &rewriter) const override {
    auto source = cast<ada::QualType>(op.getInput().getType());
    auto target = cast<ada::QualType>(op.getType());
    if (source == target) {
      rewriter.replaceOp(op, op.getInput());
      return success();
    }
    if (source.getMlirType() != target.getMlirType())
      return rewriter.notifyMatchFailure(op, "representation change");

    ada::TypeOp targetOp = ada::lookupAdaType(op, target.getAdaType());
    if (!targetOp)
      return rewriter.notifyMatchFailure(op, "unresolved target type");
    FlatSymbolRefAttr base = targetOp.getBaseAttr();
    if ((base ? base : target.getAdaType()) !=
        ada::baseOfAdaType(op, source.getAdaType()))
      return rewriter.notifyMatchFailure(op, "different or unresolved bases");

    // An unconstrained target (a base type or a pure renaming) needs no check.
    // @todo Check float and enumeration constraints; like MLIRGen, only integer
    // ranges are checked for now.
    auto required = base ? dyn_cast_or_null<ada::IntegerTypeInfoAttr>(
                               targetOp.getTypeInfoAttr())
                         : ada::IntegerTypeInfoAttr();
    if (!required) {
      rewriter.replaceOpWithNewOp<ada::RequalifyOp>(op, target, op.getInput());
      return success();
    }
    IntegerAttr lo = required.staticLower(), hi = required.staticUpper();
    if (!lo || !hi) {
      // A dynamic bound: check against the descriptor elaborated at the subtype
      // declaration, which the conversion carries.
      Value range = op.getRange();
      if (!range)
        return rewriter.notifyMatchFailure(op, "no range descriptor");
      rewriter.replaceOpWithNewOp<ada::RangeCheckOp>(op, target, op.getInput(),
                                                     range);
      return success();
    }

    // Neither does a value known to lie within the target's range.
    if (knownWithin(op, op.getInput(), ada::boundValue(lo),
                    ada::boundValue(hi))) {
      rewriter.replaceOpWithNewOp<ada::RequalifyOp>(op, target, op.getInput());
      return success();
    }

    // Otherwise check the value against the target's bounds, which have the
    // base type (@rm{3-5}). The check yields the target subtype.
    auto baseQual =
        ada::QualType::get(getContext(), target.getMlirType(), base);
    Value range =
        ada::getOrCreateStaticRange(rewriter, op.getLoc(), target.getAdaType(),
                                    baseQual, lo.getValue(), hi.getValue());
    rewriter.replaceOpWithNewOp<ada::RangeCheckOp>(op, target, op.getInput(),
                                                   range);
    return success();
  }
};

/// Lower a value conversion to another base type, which may change the machine
/// width (@rm{4-6}): a range check, a representation change to the target's
/// base type, and a check or a requalification into the target. The check goes
/// after a widening that preserves every value, so it establishes the target
/// subtype. Otherwise, as the change can destroy the information the check
/// needs, it goes first, in the source representation.
struct RepresentationChangeConversion
    : public OpRewritePattern<ada::ConvertOp> {
  using OpRewritePattern<ada::ConvertOp>::OpRewritePattern;

  LogicalResult matchAndRewrite(ada::ConvertOp op,
                                PatternRewriter &rewriter) const override {
    auto source = cast<ada::QualType>(op.getInput().getType());
    auto target = cast<ada::QualType>(op.getType());
    Type sourceType = source.getMlirType(), targetType = target.getMlirType();
    if (isa<FloatType>(sourceType) != isa<FloatType>(targetType))
      return rewriter.notifyMatchFailure(op, "integer and float conversion");

    FlatSymbolRefAttr sourceBase = ada::baseOfAdaType(op, source.getAdaType());
    FlatSymbolRefAttr targetBase = ada::baseOfAdaType(op, target.getAdaType());
    if (!sourceBase || !targetBase)
      return rewriter.notifyMatchFailure(op, "unresolved Ada type");
    if (sourceType == targetType && sourceBase == targetBase)
      return rewriter.notifyMatchFailure(op, "same representation");
    auto targetBaseQual =
        ada::QualType::get(getContext(), targetType, targetBase);

    // @todo Check float and enumeration constraints, only integer ranges are
    // checked for now.
    IntegerAttr lo, hi;
    Value dynamicRange;
    if (ada::IntegerTypeInfoAttr required =
            ada::intTypeInfoOfAdaType(op, target.getAdaType())) {
      lo = required.staticLower();
      hi = required.staticUpper();
      if (!lo || !hi) {
        // A dynamic bound: the value must first fit the target's base, then the
        // descriptor elaborated at the subtype declaration, which the
        // conversion carries.
        dynamicRange = op.getRange();
        if (!dynamicRange)
          return rewriter.notifyMatchFailure(op, "no range descriptor");
        ada::IntegerTypeInfoAttr baseInfo =
            ada::intTypeInfoOfAdaType(op, targetBase);
        lo = baseInfo ? baseInfo.staticLower() : IntegerAttr();
        hi = baseInfo ? baseInfo.staticUpper() : IntegerAttr();
        if (!lo || !hi)
          return rewriter.notifyMatchFailure(op, "non-static base range");
      }
    }

    bool sourceUnsigned =
        static_cast<bool>(ada::modulusOfAdaType(op, source.getAdaType()));
    bool check = false, preserving = false;
    if (lo) {
      // No check when the value is known to lie within the target's range.
      check = !knownWithin(op, op.getInput(), ada::boundValue(lo),
                           ada::boundValue(hi));
      bool targetUnsigned =
          static_cast<bool>(ada::modulusOfAdaType(op, targetBase));
      // Widening preserves every value, except a negative one into a modular
      // type.
      preserving = targetType.getIntOrFloatBitWidth() >
                       sourceType.getIntOrFloatBitWidth() &&
                   (sourceUnsigned || !targetUnsigned);
    }

    Location loc = op.getLoc();
    Value value = op.getInput();
    if (check && !preserving) {
      // Check first, against the target range restricted to the source
      // representation: the values that pass convert exactly.
      auto sourceBaseQual =
          ada::QualType::get(getContext(), sourceType, sourceBase);
      auto [clampedLo, clampedHi] =
          clampTo(ada::boundValue(lo), ada::boundValue(hi),
                  cast<IntegerType>(sourceType), sourceUnsigned);
      // The descriptor is named after the source base but holds the restricted
      // bounds: a check only reads the bounds.
      Value range = ada::getOrCreateStaticRange(
          rewriter, loc, sourceBase, sourceBaseQual, clampedLo, clampedHi);
      value = ada::RangeCheckOp::create(rewriter, loc, sourceBaseQual, value,
                                        range);
      check = false;
    }

    // Representation change, to the target's base type.
    value = changeRepresentation(rewriter, loc, value, targetBaseQual,
                                 sourceUnsigned);

    // Check after a preserving widening, which establishes the target subtype,
    // or against a dynamic range, or requalify.
    if (dynamicRange) {
      value =
          ada::RangeCheckOp::create(rewriter, loc, target, value, dynamicRange);
    } else if (check) {
      Value range = ada::getOrCreateStaticRange(
          rewriter, loc, target.getAdaType(), targetBaseQual, lo.getValue(),
          hi.getValue());
      value = ada::RangeCheckOp::create(rewriter, loc, target, value, range);
    } else if (value.getType() != target) {
      value = ada::RequalifyOp::create(rewriter, loc, target, value);
    }
    rewriter.replaceOp(op, value);
    return success();
  }
};

struct LowerAdaConversionsPass
    : public PassWrapper<LowerAdaConversionsPass, OperationPass<ModuleOp>> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(LowerAdaConversionsPass)
  void getDependentDialects(DialectRegistry &registry) const override {
    registry.insert<arith::ArithDialect>();
  }
  void runOnOperation() final;
  StringRef getName() const final { return "LowerAdaConversions"; }
  StringRef getArgument() const final { return "ada-lower-conversions"; }
  StringRef getDescription() const final {
    return "Expand ada.convert into checks, casts and qualification";
  }
};
} // namespace

void LowerAdaConversionsPass::runOnOperation() {
  // No ada.convert is legal after the pass.
  ConversionTarget target(getContext());
  target.addLegalDialect<ada::AdaDialect, arith::ArithDialect>();
  target.addIllegalOp<ada::ConvertOp>();

  RewritePatternSet patterns(&getContext());
  patterns.add<SameRepresentationConversion, RepresentationChangeConversion>(
      &getContext());

  if (failed(
          applyPartialConversion(getOperation(), target, std::move(patterns))))
    signalPassFailure();
}

std::unique_ptr<mlir::Pass> mlir::ada::createLowerAdaConversionsPass() {
  return std::make_unique<LowerAdaConversionsPass>();
}
