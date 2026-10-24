// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

#include "ada/Dialect.h"
#include "ada/Passes.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Support/TypeID.h"
#include "mlir/Transforms/DialectConversion.h"

using namespace mlir;

/// Whether `knownLo..knownHi` lies within `lo..hi`.
static bool rangeWithin(const APInt &knownLo, const APInt &knownHi,
                        const APInt &lo, const APInt &hi, bool isUnsigned) {
  return ada::inStaticRange(knownLo, lo, hi, isUnsigned) &&
         ada::inStaticRange(knownHi, lo, hi, isUnsigned);
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

    // Neither does a source whose static range lies within the target's.
    if (ada::IntegerTypeInfoAttr known =
            ada::intTypeInfoOfAdaType(op, source.getAdaType())) {
      IntegerAttr knownLo = known.staticLower(), knownHi = known.staticUpper();
      bool isUnsigned = static_cast<bool>(ada::modulusOfAdaType(op, base));
      if (knownLo && knownHi &&
          rangeWithin(knownLo.getValue(), knownHi.getValue(), lo.getValue(),
                      hi.getValue(), isUnsigned)) {
        rewriter.replaceOpWithNewOp<ada::RequalifyOp>(op, target,
                                                      op.getInput());
        return success();
      }
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

struct LowerAdaConversionsPass
    : public PassWrapper<LowerAdaConversionsPass, OperationPass<ModuleOp>> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(LowerAdaConversionsPass)
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
  target.addLegalDialect<ada::AdaDialect>();
  target.addIllegalOp<ada::ConvertOp>();

  RewritePatternSet patterns(&getContext());
  patterns.add<SameRepresentationConversion>(&getContext());

  if (failed(
          applyPartialConversion(getOperation(), target, std::move(patterns))))
    signalPassFailure();
}

std::unique_ptr<mlir::Pass> mlir::ada::createLowerAdaConversionsPass() {
  return std::make_unique<LowerAdaConversionsPass>();
}
