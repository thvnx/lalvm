// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

#ifndef ADA_ADATOLLVMIRTRANSLATION_H
#define ADA_ADATOLLVMIRTRANSLATION_H

namespace mlir {
class DialectRegistry;
class MLIRContext;
namespace ada {

/// Register the Ada dialect translation interface so that surviving metadata
/// ops (such as ada.type) are dropped during MLIR-to-LLVM IR translation.
void registerAdaDialectTranslation(DialectRegistry &registry);
void registerAdaDialectTranslation(MLIRContext &context);

} // namespace ada
} // namespace mlir

#endif // ADA_ADATOLLVMIRTRANSLATION_H
