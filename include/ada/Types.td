//===- Types.td - Ada dialect type definitions ---------------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Defines the types of the Ada dialect.
//
//===----------------------------------------------------------------------===//

#ifndef ADA_TYPES
#define ADA_TYPES

include "mlir/IR/AttrTypeBase.td"
include "mlir/IR/BuiltinTypeInterfaces.td"
include "Dialect.td"

//===----------------------------------------------------------------------===//
// Ada base type class
//===----------------------------------------------------------------------===//

class Ada_Type<string name, string typeMnemonic, list<Trait> traits = []>
    : TypeDef<Ada_Dialect, name, traits> {
  let mnemonic = typeMnemonic;
}

//===----------------------------------------------------------------------===//
// QualType
//===----------------------------------------------------------------------===//

// Wraps an MLIR type with an Ada type symbol, making Ada type identity
// first-class in the MLIR type system. Values of this type carry both their
// machine representation (mlirType) and their Ada declared type (@adaType, a
// symbol reference to the relevant ada.type op). Implements
// MemRefElementTypeInterface so memref<!ada.qual<T>> is a valid (and textually
// parseable) type; MemRefType's verifier gates on it.
def Ada_QualType : Ada_Type<"Qual", "qual", [MemRefElementTypeInterface]> {
  let summary = "Ada value type: machine type annotated with Ada type identity";
  let parameters = (ins "::mlir::Type":$mlirType,
                        "::mlir::FlatSymbolRefAttr":$adaType);
  let assemblyFormat = "`<` $mlirType `,` $adaType `>`";
}

//===----------------------------------------------------------------------===//
// RangeType
//===----------------------------------------------------------------------===//

// The type of an `ada.range` value: a subtype's constraint made first-class.
// `boundType` is the bare machine type of the bounds (`iN`/`fN`); the Ada type
// identity appears once, in `@constrainedType`, the subtype the range
// constrains. Unlike `ada.qual`, the bounds carry no per-value Ada identity:
// a range *is* the descriptor of one subtype. Lowers to
// `!llvm.struct<(boundType, boundType)>`.
def Ada_RangeType : Ada_Type<"Range", "range"> {
  let summary = "Ada subtype constraint descriptor: a (low, high) bound pair";
  let parameters = (ins "::mlir::Type":$boundType,
                        "::mlir::FlatSymbolRefAttr":$constrainedType);
  let assemblyFormat = "`<` $boundType `,` $constrainedType `>`";
}

//===----------------------------------------------------------------------===//
// ArrayType
//===----------------------------------------------------------------------===//

// The machine layout of an Ada array type: a component machine type plus, per
// dimension, an index machine type and an extent (an element count;
// `mlir::ShapedType::kDynamic` is the `?` sentinel for a dynamic extent). The
// Ada type identity lives in the `array_info` attribute on the `ada.type` op.
def Ada_ArrayType : Ada_Type<"Array", "array"> {
  let summary = "Ada array type: component plus per-dimension index and extent";
  let parameters = (ins "::mlir::Type":$componentType,
                        ArrayRefParameter<"::mlir::Type">:$indexTypes,
                        ArrayRefParameter<"int64_t">:$extents);
  // Printed as `!ada.array<componentType[indexType x extent, ...]>`
  // (e.g. `!ada.array<i32[i32 x 10]>` for `array (1 .. 10) of Integer`).
  let hasCustomAssemblyFormat = 1;
  // Checks rank consistency (`indexTypes` and `extents` agree in length),
  // extents non-negative or dynamic, and integer index types.
  let genVerifyDecl = 1;
}

#endif // ADA_TYPES
