//===- PostTranslationDITypes.h - Post-translation DI type builders -------===//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project
//
//===----------------------------------------------------------------------===//
//
// DI type builders running on the llvm::Module after translateModuleToLLVMIR.
// MLIR 23 still has no DIEnumeratorAttr or DISubrangeTypeAttr, so
// AdaDebugInfoPass emits placeholder types that these builders replace with the
// full nodes, built through LLVM's DIBuilder.
//
// @todo Retire once MLIR gains both attrs: the types can then be built in
// AdaDebugInfoPass.
//
//===----------------------------------------------------------------------===//

#ifndef ADA_POSTTRANSLATIONDITYPES_H
#define ADA_POSTTRANSLATIONDITYPES_H

#include <cstdint>
#include <optional>

namespace llvm {
class Module;
} // namespace llvm

namespace mlir {
class ModuleOp;
class Type;
} // namespace mlir

namespace mlir::ada {

/// Size and DWARF encoding (DW_ATE_*) of a DW_TAG_base_type.
struct DIBasicEncoding {
  uint64_t sizeInBits;
  unsigned encoding;
};

/// The DW_TAG_base_type encoding of a numeric Ada type of machine type
/// `mlirType`, unsigned when `modular`, or none for a non-numeric type.
/// AdaDebugInfoPass and these builders share it, so that DIBuilder returns
/// the very node the pass's attribute translated to.
std::optional<DIBasicEncoding> basicTypeEncoding(mlir::Type mlirType,
                                                 bool modular);

/// Complete the enumeration, subrange and array debug types after LLVM
/// translation.
void buildPostTranslationDITypes(llvm::Module &llvmModule,
                                 mlir::ModuleOp module);

} // namespace mlir::ada

#endif // ADA_POSTTRANSLATIONDITYPES_H
