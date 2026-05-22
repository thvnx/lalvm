//===- ToLLVMIRTranslation.h - Ada dialect LLVM IR translation ---*- C++
//-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef ADA_TOLLVMIRTRANSLATION_H
#define ADA_TOLLVMIRTRANSLATION_H

namespace mlir {
class DialectRegistry;
class MLIRContext;
namespace ada {

/// Registers the Ada dialect translation interface so that surviving
/// metadata ops (currently only ada.type) are dropped during MLIR-to-LLVM IR
/// translation instead of causing a "cannot convert op" error.
void registerAdaDialectTranslation(DialectRegistry &registry);
void registerAdaDialectTranslation(MLIRContext &context);

} // namespace ada
} // namespace mlir

#endif // ADA_TOLLVMIRTRANSLATION_H
