//===- Passes.h - Ada Passes Definition -----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file exposes the entry points to create compiler passes for Ada.
//
//===----------------------------------------------------------------------===//

#ifndef ADA_PASSES_H
#define ADA_PASSES_H

#include <memory>

namespace mlir {
class Pass;

namespace ada {

/// Create a pass for lowering Ada dialect operations to the LLVM dialect.
std::unique_ptr<mlir::Pass> createLowerToLLVMPass();

} // namespace ada
} // namespace mlir

#endif // ADA_PASSES_H
