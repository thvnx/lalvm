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
  auto known = info ? info.staticBounds() : std::nullopt;
  return known && rangeWithin(known->first, known->second, lo, hi);
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
/// precision. The machine value is kept as is between the same types. The cast,
/// the only new value, keeps the object name of `loc` with the Ada type
/// `nameType` for debug info.
static Value changeRepresentation(PatternRewriter &rewriter, Location loc,
                                  Value value, ada::QualType targetQual,
                                  bool sourceUnsigned,
                                  FlatSymbolRefAttr nameType) {
  Type sourceType = cast<ada::QualType>(value.getType()).getMlirType();
  Type targetType = targetQual.getMlirType();
  Location unnamed = ada::unnamedLoc(loc);
  Location castLoc = ada::attachAdaTypeRef(loc, nameType);
  Value raw = ada::UnwrapOp::create(rewriter, unnamed, sourceType, value);
  unsigned sourceWidth = sourceType.getIntOrFloatBitWidth();
  unsigned targetWidth = targetType.getIntOrFloatBitWidth();
  if (isa<FloatType>(sourceType)) {
    if (targetWidth < sourceWidth)
      raw = arith::TruncFOp::create(rewriter, castLoc, targetType, raw);
    else if (targetWidth > sourceWidth)
      raw = arith::ExtFOp::create(rewriter, castLoc, targetType, raw);
  } else if (targetWidth < sourceWidth) {
    raw = arith::TruncIOp::create(rewriter, castLoc, targetType, raw);
  } else if (targetWidth > sourceWidth) {
    if (sourceUnsigned)
      raw = arith::ExtUIOp::create(rewriter, castLoc, targetType, raw);
    else
      raw = arith::ExtSIOp::create(rewriter, castLoc, targetType, raw);
  }
  return ada::WrapOp::create(rewriter, unnamed, targetQual, raw);
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
    auto bounds = required.staticBounds();
    if (!bounds) {
      // A dynamic bound: check against the descriptor elaborated at the subtype
      // declaration, which the conversion carries.
      Value range = op.getRange();
      if (!range)
        return rewriter.notifyMatchFailure(op, "no range descriptor");
      rewriter.replaceOpWithNewOp<ada::RangeCheckOp>(op, target, op.getInput(),
                                                     range);
      return success();
    }

    // A value known to lie within the target's range needs no check either.
    auto [lo, hi] = *bounds;
    if (knownWithin(op, op.getInput(), lo, hi)) {
      rewriter.replaceOpWithNewOp<ada::RequalifyOp>(op, target, op.getInput());
      return success();
    }

    // Otherwise check the value against the target's bounds, which have the
    // base type (@rm{3-5}). The check yields the target subtype.
    auto baseQual =
        ada::QualType::get(getContext(), target.getMlirType(), base);
    Value range =
        ada::getOrCreateStaticRange(rewriter, ada::unnamedLoc(op.getLoc()),
                                    target.getAdaType(), baseQual, lo, hi);
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
    // @todo Lower conversions between integer and float types (@rm{4-6}(33)).
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
    std::optional<std::pair<APSInt, APSInt>> bounds;
    Value dynamicRange;
    if (ada::IntegerTypeInfoAttr required =
            ada::intTypeInfoOfAdaType(op, target.getAdaType())) {
      bounds = required.staticBounds();
      if (!bounds) {
        // A dynamic bound: the value must first fit the target's base, then the
        // descriptor elaborated at the subtype declaration, which the
        // conversion carries.
        dynamicRange = op.getRange();
        if (!dynamicRange)
          return rewriter.notifyMatchFailure(op, "no range descriptor");
        ada::IntegerTypeInfoAttr baseInfo =
            ada::intTypeInfoOfAdaType(op, targetBase);
        if (baseInfo)
          bounds = baseInfo.staticBounds();
        if (!bounds)
          return rewriter.notifyMatchFailure(op, "non-static base range");
      }
    }

    bool sourceUnsigned =
        static_cast<bool>(ada::modulusOfAdaType(op, source.getAdaType()));
    bool check = false, preserving = false;
    if (bounds) {
      // No check when the value is known to lie within the target's range.
      check = !knownWithin(op, op.getInput(), bounds->first, bounds->second);
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
          clampTo(bounds->first, bounds->second, cast<IntegerType>(sourceType),
                  sourceUnsigned);
      // The descriptor is named after the source base but holds the restricted
      // bounds: a check only reads the bounds.
      Location unnamed = ada::unnamedLoc(loc);
      Value range = ada::getOrCreateStaticRange(
          rewriter, unnamed, sourceBase, sourceBaseQual, clampedLo, clampedHi);
      value = ada::RangeCheckOp::create(rewriter, unnamed, sourceBaseQual,
                                        value, range);
      check = false;
    }

    // Representation change, to the target's base type.
    value = changeRepresentation(rewriter, loc, value, targetBaseQual,
                                 sourceUnsigned, target.getAdaType());

    // Check after a preserving widening, which establishes the target subtype,
    // or against a dynamic range, or requalify.
    if (dynamicRange) {
      value =
          ada::RangeCheckOp::create(rewriter, loc, target, value, dynamicRange);
    } else if (check) {
      Value range = ada::getOrCreateStaticRange(
          rewriter, ada::unnamedLoc(loc), target.getAdaType(), targetBaseQual,
          bounds->first, bounds->second);
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
