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

/// Create a pass that lambda-lifts up-level references in nested subprograms:
/// each value captured from an enclosing scope becomes an explicit parameter,
/// call sites are rewritten to pass it, and the now self-contained subprogram
/// is hoisted to module level. Must run before `mem2reg` so captured locals
/// stay allocas that can carry up-level writes.
std::unique_ptr<mlir::Pass> createClosureConversionPass();

/// Create a pass that applies GNAT ABI name mangling to subprograms (now at
/// module level after closure conversion) and hoists the remaining nested Ada
/// `ada.type` ops to module level, erasing the emptied `ada.decls`. Must run
/// before LowerToLLVM.
std::unique_ptr<mlir::Pass> createHoistNestedSymbolOperationsPass();

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
