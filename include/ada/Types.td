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
// machine representation (mlirType) and their Ada declared type (@adaType,
// a symbol reference to the relevant ada.type op).
def Ada_QualType : Ada_Type<"Qual", "qual"> {
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

#endif // ADA_TYPES
