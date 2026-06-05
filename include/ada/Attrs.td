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

// Reference to an ada.type op by sym_name, for use as FusedLoc metadata in
// value locations. Embedded by LowerToLLVM's attachAdaTypeRef so that
// AdaDebugInfoPass can resolve the correct DIType for each value.
def DITypeRefAttr : AttrDef<Ada_Dialect, "DITypeRef"> {
  let mnemonic = "typeref";
  let summary = "Ada type cross-reference for FusedLoc metadata";
  let parameters = (ins
    AttrParameter<"::mlir::FlatSymbolRefAttr", "ada.type sym_name">:$sym
  );
  let assemblyFormat = "`<` $sym `>`";
}

// Reference to an enclosing subprogram, carried as FusedLoc metadata to record
// the DWARF scope of an op that has been moved away from its lexical parent.
// Lets a later pass recover the original scope from the location alone, after
// the IR no longer reflects it through nesting.
def DIScopeRefAttr : AttrDef<Ada_Dialect, "DIScopeRef"> {
  let mnemonic = "di_scope";
  let summary = "Enclosing-subprogram DWARF scope for FusedLoc metadata";
  let parameters = (ins
    AttrParameter<"::mlir::FlatSymbolRefAttr", "enclosing subprogram sym_name">:$scope
  );
  let assemblyFormat = "`<` $scope `>`";
}

// Bare Ada source name of a subprogram, carried as FusedLoc metadata so that
// AdaDebugInfoPass can set DW_AT_name to the source name while DW_AT_linkage_name
// keeps the GNAT-mangled symbol. Captured by HoistNestedSymbolOperations before
// it overwrites the sym_name with the mangled form (after which the bare name is
// no longer recoverable).
def DINameAttr : AttrDef<Ada_Dialect, "DIName"> {
  let mnemonic = "di_name";
  let summary = "Bare Ada source name for FusedLoc metadata";
  let parameters = (ins
    AttrParameter<"::mlir::StringAttr", "bare Ada source name">:$name
  );
  let assemblyFormat = "`<` $name `>`";
}

#endif // ADA_ATTRS
