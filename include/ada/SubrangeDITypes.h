//===- SubrangeDITypes.h - Post-translation subrange DI type builder -----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Declares buildSubrangeDITypes, a post-translation step that builds
// DW_TAG_subrange_type nodes via LLVM's DIBuilder API and patches the
// DIDerivedType typedef placeholders emitted by AdaDebugInfoPass for
// constrained integer subtypes.
//
// @todo Remove once MLIR gains a DISubrangeType attr; at that point the
// subrange type can be built directly inside AdaDebugInfoPass and this file can
// be retired.
//
//===----------------------------------------------------------------------===//

#ifndef ADA_SUBRANGEDITYPES_H
#define ADA_SUBRANGEDITYPES_H

#include "mlir/IR/BuiltinOps.h"

namespace llvm {
class Module;
} // namespace llvm

namespace mlir::ada {

/// Build DW_TAG_subrange_type nodes for every constrained integer subtype
/// `ada.type` op (with a range, static or dynamic) surviving in `module`, then
/// replace the DIDerivedType typedef placeholders emitted by AdaDebugInfoPass
/// with the full types. Uses LLVM's DIBuilder API directly since MLIR 21 lacks
/// a DISubrangeType attr. Must be called after translateModuleToLLVMIR. The
/// bound values come from the subtype's `int_info`; the underlying type DIE
/// from its `base` link.
void buildSubrangeDITypes(llvm::Module &llvmModule, mlir::ModuleOp module);

} // namespace mlir::ada

#endif // ADA_SUBRANGEDITYPES_H
