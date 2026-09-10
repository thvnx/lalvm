// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

#include "ada/AdaToLLVMIRTranslation.h"
#include "ada/Dialect.h"
#include "mlir/IR/DialectRegistry.h"
#include "mlir/Target/LLVMIR/LLVMTranslationInterface.h"

using namespace mlir;

namespace {
class AdaDialectLLVMIRTranslationInterface
    : public LLVMTranslationDialectInterface {
public:
  using LLVMTranslationDialectInterface::LLVMTranslationDialectInterface;

  LogicalResult convertOperation(Operation *op, llvm::IRBuilderBase &,
                                 LLVM::ModuleTranslation &) const override {
    // ada.type has no LLVM IR representation (it holds metadata that are no
    // longer useful when translating to LLVM IR). Drop it silently.
    if (isa<ada::TypeOp>(op))
      return success();
    return op->emitError("unexpected Ada op surviving to LLVM IR translation");
  }
};
} // namespace

void mlir::ada::registerAdaDialectTranslation(DialectRegistry &registry) {
  registry.addExtension(+[](MLIRContext *ctx, ada::AdaDialect *dialect) {
    dialect->addInterfaces<AdaDialectLLVMIRTranslationInterface>();
  });
}

void mlir::ada::registerAdaDialectTranslation(MLIRContext &context) {
  DialectRegistry registry;
  registerAdaDialectTranslation(registry);
  context.appendDialectRegistry(registry);
}
