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

/// Create a pass that hoists nested Ada subprograms to module level and
/// applies GNAT ABI name mangling. Must run before LowerToLLVM.
std::unique_ptr<mlir::Pass> createHoistNestedSubprogramsPass();

/// Create a pass for lowering Ada dialect operations to the LLVM dialect.
std::unique_ptr<mlir::Pass> createLowerToLLVMPass();

/// Create a pass that attaches an Ada DICompileUnitAttr to the module location
/// so that DIScopeForLLVMFuncOpPass uses Ada language metadata instead of the
/// default DW_LANG_C / "MLIR". Must run before DIScopeForLLVMFuncOpPass.
std::unique_ptr<mlir::Pass> createDICompileUnitAdaPass();

/// Create a pass that emits LLVM debug intrinsics for Ada objects and
/// parameters: `dbg.declare` for allocas and reference parameters,
/// `dbg.value` for scalars, named numbers, and value parameters. Must run
/// after DIScopeForLLVMFuncOpPass so that DISubprogramAttr is available on
/// each llvm.func.
///
/// Enum types use an empty `DICompositeTypeAttr` stub; `buildEnumDITypes` in
/// EnumDITypes.h replaces those stubs with full `DW_TAG_enumeration_type`
/// nodes via LLVM's DIBuilder after MLIR-to-LLVM translation.
///
/// @todo Remove the stub/replace dance once MLIR gains `DIEnumeratorAttr`
/// support; at that point enum composite types can be built directly inside
/// this pass and `buildEnumDITypes` can be retired.
std::unique_ptr<mlir::Pass> createAdaDebugInfoPass();

} // namespace ada
} // namespace mlir

#endif // ADA_PASSES_H
