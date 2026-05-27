//===- Attrs.td - Ada dialect attribute definitions ---------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Defines the attributes of the Ada dialect.
//
//===----------------------------------------------------------------------===//

#ifndef ADA_ATTRS
#define ADA_ATTRS

include "mlir/IR/AttrTypeBase.td"
include "mlir/IR/EnumAttr.td"
include "Dialect.td"

//===----------------------------------------------------------------------===//
// Ada Type Info Attributes
//===----------------------------------------------------------------------===//

// Carries enumerator names and representation values for an enumeration type.
def EnumTypeInfoAttr : AttrDef<Ada_Dialect, "EnumTypeInfo"> {
  let mnemonic = "enum_info";
  let summary = "Enumeration type metadata";
  let parameters = (ins
    AttrOrTypeParameter<"::mlir::ArrayAttr", "enumerator names, lowercased">:$names,
    ArrayRefParameter<"int64_t", "integer representation values">:$values
  );
  let extraClassDeclaration = [{
    /// Returns the integer representation value for the given enumerator name,
    /// or std::nullopt if not found.
    std::optional<int64_t> enumRep(::llvm::StringRef name) const;
    /// Returns a zip range of (StringRef name, int64_t value) pairs.
    auto literals() const {
      return llvm::zip_equal(getNames().getAsValueRange<::mlir::StringAttr>(),
                             getValues());
    }
  }];
  let description = [{
    Carries the enumerator names and their integer representation values for
    an Ada enumeration type. Names are stored lowercased to match Ada's
    case-insensitivity rules.

    Syntax:

        <"name1" = val1, "name2" = val2, ...>
  }];
  let hasCustomAssemblyFormat = 1;
}

// Marker attribute for numeric types. Reserved for future range-check metadata.
def NumericTypeInfoAttr : AttrDef<Ada_Dialect, "NumericTypeInfo"> {
  let mnemonic = "numeric_info";
  let summary = "Numeric type metadata (integer or float)";
  let parameters = (ins
    OptionalParameter<"uint64_t">:$modulus
  );
  let assemblyFormat = "(`<` `mod` `=` $modulus^ `>`)?";
}

#endif // ADA_ATTRS
