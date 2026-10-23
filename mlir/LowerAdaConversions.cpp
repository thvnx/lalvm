// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

#include "ada/Dialect.h"
#include "ada/Passes.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Support/TypeID.h"
#include "mlir/Transforms/DialectConversion.h"

using namespace mlir;

namespace {
struct LowerAdaConversionsPass
    : public PassWrapper<LowerAdaConversionsPass, OperationPass<ModuleOp>> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(LowerAdaConversionsPass)
  void runOnOperation() final;
  StringRef getArgument() const final { return "ada-lower-conversions"; }
  StringRef getDescription() const final {
    return "Expand ada.convert into checks, casts and qualification";
  }
};
} // namespace

void LowerAdaConversionsPass::runOnOperation() {
  // No ada.convert is legal after the pass; other ops are left untouched.
  ConversionTarget target(getContext());
  target.addIllegalOp<ada::ConvertOp>();

  // @todo Add the ada.convert lowering patterns.
  RewritePatternSet patterns(&getContext());

  if (failed(
          applyPartialConversion(getOperation(), target, std::move(patterns))))
    signalPassFailure();
}

std::unique_ptr<mlir::Pass> mlir::ada::createLowerAdaConversionsPass() {
  return std::make_unique<LowerAdaConversionsPass>();
}
