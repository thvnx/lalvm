//===- PostTranslationDITypes.h - Post-translation DI type builders -------===//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project
//
//===----------------------------------------------------------------------===//
//
// DI type builders running on the llvm::Module after translateModuleToLLVMIR.
// MLIR 22 has no DIEnumeratorAttr and no DISubrangeType attr, so
// AdaDebugInfoPass emits placeholder types that these builders replace with the
// full nodes, built through LLVM's DIBuilder. Run them in the order below: the
// array builder reuses the nodes of the first two.
//
// In the examples, `!p` is the enclosing subprogram and `!integer` the
// DW_TAG_base_type of Standard.Integer.
//
// @todo Retire once MLIR gains both attrs: the types can then be built in
// AdaDebugInfoPass.
//
//===----------------------------------------------------------------------===//

#ifndef ADA_POSTTRANSLATIONDITYPES_H
#define ADA_POSTTRANSLATIONDITYPES_H

#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinOps.h"
#include "llvm/ADT/DenseMap.h"

namespace llvm {
class DIType;
class Module;
} // namespace llvm

namespace mlir::ada {

/// Nodes built by a builder, keyed by the `ada.type` sym_name.
using DITypeBySymName = llvm::DenseMap<mlir::StringAttr, llvm::DIType *>;

/// Build a DW_TAG_enumeration_type for each surviving enum `ada.type` op and
/// substitute it for the op's empty DICompositeType stub. A local type is
/// scoped to its subprogram. For `type Color is (Red, Green)`:
///
///   !DICompositeType(tag: DW_TAG_enumeration_type, name: "p.color", size: 8)
///
/// becomes
///
///   !DICompositeType(tag: DW_TAG_enumeration_type, name: "color", scope: !p,
///       size: 8, elements: !{!red, !green})
///   !red = !DIEnumerator(name: "red", value: 0)
///   !green = !DIEnumerator(name: "green", value: 1)
DITypeBySymName buildEnumDITypes(llvm::Module &llvmModule,
                                 mlir::ModuleOp module);

/// Build a DW_TAG_subrange_type for each surviving constrained integer subtype
/// (static or dynamic range) and substitute it for the op's typedef stub. The
/// bounds come from `int_info`, the underlying type from the `base` link. For
/// `subtype Small is Integer range 1 .. 10`:
///
///   !DIDerivedType(tag: DW_TAG_typedef, name: "p.small", baseType: !integer)
///
/// becomes
///
///   !DISubrangeType(name: "small", scope: !p, size: 32, baseType: !integer,
///       lowerBound: i32 1, upperBound: i32 10)
DITypeBySymName buildSubrangeDITypes(llvm::Module &llvmModule,
                                     mlir::ModuleOp module);

/// Build a DW_TAG_array_type for each surviving statically constrained array
/// and substitute it for the op's empty DICompositeType stub. The element type
/// is the component's node in `types`, else the DW_TAG_base_type of its numeric
/// base; each dimension is a DW_TAG_subrange_type of the index's numeric base
/// with the bounds. For `type Vec is array (5 .. 14) of Integer`:
///
///   !DICompositeType(tag: DW_TAG_array_type, name: "p.vec",
///       baseType: !integer, size: 320)
///
/// becomes
///
///   !DICompositeType(tag: DW_TAG_array_type, name: "vec", scope: !p,
///       baseType: !integer, size: 320, elements: !{!dim})
///   !dim = !DISubrangeType(scope: !p, size: 32, baseType: !integer,
///       lowerBound: i32 5, upperBound: i32 14)
void buildArrayDITypes(llvm::Module &llvmModule, mlir::ModuleOp module,
                       const DITypeBySymName &types);

} // namespace mlir::ada

#endif // ADA_POSTTRANSLATIONDITYPES_H
