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
#include <optional>
#include <string>

namespace mlir {
class Pass;

namespace ada {

/// Metadata for one Ada enumeration type, collected from an `ada.type` op
/// before it is erased by LowerToLLVM. Consumed by `attachAdaDebugInfo`
/// in lalvm.cpp to emit `DW_TAG_enumeration_type` via LLVM's DIBuilder.
struct AdaEnumInfo {
  std::string typeName;
  unsigned bitWidth;
  mlir::Location loc;
  llvm::SmallVector<std::string> names; // lowercased
  llvm::SmallVector<int64_t> values;
  /// Mangled LLVM name of the enclosing subprogram, if this type is declared
  /// locally inside a subprogram. nullopt for module-level types.
  std::optional<std::string> subpScope;
};

/// Metadata for the Ada-typed parameters of one subprogram, collected from
/// `"ada.type"` arg_attrs on an `ada.subp` op. Consumed by
/// `attachAdaDebugInfo` in lalvm.cpp to emit `DW_TAG_formal_parameter`.
struct AdaParamInfo {
  std::string subpScope; // mangled LLVM name of the owning subprogram
  struct Param {
    std::string name;
    std::string typeName;
    mlir::Location loc;
    unsigned argIndex;
  };
  llvm::SmallVector<Param> params;
};

/// Create a pass that collects Ada type metadata into `enumInfos` and
/// `paramInfos` before `ada.type` ops are erased by LowerToLLVM. No IR
/// mutations.
std::unique_ptr<mlir::Pass>
createAddAdaDebugInfoPass(llvm::SmallVector<AdaEnumInfo> &enumInfos,
                          llvm::SmallVector<AdaParamInfo> &paramInfos);

/// Create a pass for lowering Ada dialect operations to the LLVM dialect.
std::unique_ptr<mlir::Pass> createLowerToLLVMPass();

/// Create a pass that finalizes ada.object ops: emits LLVM debug intrinsics
/// (llvm.dbg.declare / llvm.dbg.value) and erases the ops. Must run after
/// DIScopeForLLVMFuncOpPass so that DISubprogramAttr is available on each
/// llvm.func.
std::unique_ptr<mlir::Pass> createFinalizeAdaObjectPass();

} // namespace ada
} // namespace mlir

#endif // ADA_PASSES_H
