//===- EnumDITypes.h - Post-translation enum DI type builder --------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Declares buildEnumDITypes, a post-translation step that builds
// DW_TAG_enumeration_type nodes via LLVM's DIBuilder API and patches the
// empty DICompositeType stubs emitted by AdaDebugInfoPass.
//
// @todo Remove once MLIR gains DIEnumeratorAttr support; at that point enum
// composite types can be built directly inside AdaDebugInfoPass and this
// file can be retired.
//
//===----------------------------------------------------------------------===//

#ifndef ADA_ENUMDITYPES_H
#define ADA_ENUMDITYPES_H

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

} // namespace mlir::ada

#endif // ADA_ENUMDITYPES_H
