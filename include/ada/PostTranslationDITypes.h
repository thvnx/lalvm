//===- PostTranslationDITypes.h - Post-translation DI type builders -------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Declares the post-translation DI type builders that run on the llvm::Module
// after translateModuleToLLVMIR. Both build types via LLVM's DIBuilder API
// (MLIR 21 lacks DIEnumeratorAttr and a DISubrangeType attr) and patch the
// placeholder DIEs emitted by AdaDebugInfoPass:
//
//   - buildEnumDITypes:     DW_TAG_enumeration_type, replacing the empty
//                           DICompositeType stubs for enum types.
//   - buildSubrangeDITypes: DW_TAG_subrange_type, replacing the DIDerivedType
//                           typedef placeholders for constrained integer
//                           subtypes.
//
// @todo Remove once MLIR gains DIEnumeratorAttr and a DISubrangeType attr; at
// that point both can be built directly inside AdaDebugInfoPass and this file
// can be retired.
//
//===----------------------------------------------------------------------===//

#ifndef ADA_POSTTRANSLATIONDITYPES_H
#define ADA_POSTTRANSLATIONDITYPES_H

#include "mlir/IR/BuiltinOps.h"

namespace llvm {
class Module;
} // namespace llvm

namespace mlir::ada {

/// Build DW_TAG_enumeration_type nodes for every enum `ada.type` op surviving
/// in `module`, then replace the empty DICompositeType stubs emitted by
/// AdaDebugInfoPass with the full types. Uses LLVM's DIBuilder API directly
/// since MLIR 21 lacks DIEnumeratorAttr. Must be called after
/// translateModuleToLLVMIR. Locally-declared types use the enclosing
/// DISubprogram as scope instead of the compile unit.
void buildEnumDITypes(llvm::Module &llvmModule, mlir::ModuleOp module);

/// Build DW_TAG_subrange_type nodes for every constrained integer subtype
/// `ada.type` op (with a range, static or dynamic) surviving in `module`, then
/// replace the DIDerivedType typedef placeholders emitted by AdaDebugInfoPass
/// with the full types. Uses LLVM's DIBuilder API directly since MLIR 21 lacks
/// a DISubrangeType attr. Must be called after translateModuleToLLVMIR. The
/// bound values come from the subtype's `int_info`; the underlying type DIE
/// from its `base` link.
void buildSubrangeDITypes(llvm::Module &llvmModule, mlir::ModuleOp module);

} // namespace mlir::ada

#endif // ADA_POSTTRANSLATIONDITYPES_H
