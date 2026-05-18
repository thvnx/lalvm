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

#include "mlir/IR/Location.h"
#include "llvm/ADT/SmallVector.h"
#include <memory>
#include <string>

namespace mlir {
class Pass;

namespace ada {

/// Metadata for one Ada enumeration type, collected from an `ada.type` op
/// before it is erased by LowerToLLVM. Consumed by `attachEnumDebugInfo`
/// in lalvm.cpp to emit `DW_TAG_enumeration_type` via LLVM's DIBuilder.
struct AdaEnumInfo {
  std::string typeName;
  unsigned bitWidth;
  mlir::Location loc;
  llvm::SmallVector<std::string> names; // lowercased
  llvm::SmallVector<int64_t> values;
};

/// Create a pass that collects Ada enumeration type metadata into `enumInfos`
/// before `ada.type` ops are erased by LowerToLLVM. No IR mutations.
std::unique_ptr<mlir::Pass>
createAddAdaDebugInfoPass(llvm::SmallVector<AdaEnumInfo> &enumInfos);

/// Create a pass for lowering Ada dialect operations to the LLVM dialect.
std::unique_ptr<mlir::Pass> createLowerToLLVMPass();

} // namespace ada
} // namespace mlir

#endif // ADA_PASSES_H
