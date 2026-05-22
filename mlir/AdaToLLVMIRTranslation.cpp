//===- AdaToLLVMIRTranslation.cpp - Ada dialect LLVM IR translation -------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ada/Dialect.h"
#include "ada/ToLLVMIRTranslation.h"
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
    // ada.type has no LLVM IR representation; drop it silently.
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
