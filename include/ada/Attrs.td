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
// Ada Scalar Encoding
//===----------------------------------------------------------------------===//

def Ada_ScalarEncSigned   : I32EnumAttrCase<"Signed",   0, "signed">;
def Ada_ScalarEncUnsigned : I32EnumAttrCase<"Unsigned", 1, "unsigned">;
def Ada_ScalarEncFloat    : I32EnumAttrCase<"Float",    2, "float">;

def Ada_ScalarEncoding : I32EnumAttr<"ScalarEncoding",
    "scalar type encoding (RM 3.5, 3.5.6, 3.5.7)",
    [Ada_ScalarEncSigned, Ada_ScalarEncUnsigned, Ada_ScalarEncFloat]> {
  let cppNamespace = "mlir::ada";
  let genSpecializedAttr = 0;
}

//===----------------------------------------------------------------------===//
// Ada Type Info Attributes
//===----------------------------------------------------------------------===//

// Like ArrayRefParameter<"llvm::StringRef">, but also deep-copies the string
// bytes into the attribute storage allocator. Plain ArrayRefParameter copies
// only the StringRef objects (pointer+length pairs); the bytes must be
// separately owned or the StringRefs dangle after construction.
def StringRefArrayParameter :
    AttrOrTypeParameter<"::llvm::ArrayRef<::llvm::StringRef>", "array of strings"> {
  let allocator = [{
    ::llvm::SmallVector<::llvm::StringRef> strs;
    strs.reserve($_self.size());
    for (::llvm::StringRef s : $_self)
      strs.push_back($_allocator.copyInto(s));
    $_dst = $_allocator.copyInto(::llvm::ArrayRef<::llvm::StringRef>(strs));
  }];
  let comparator = [{$_lhs == $_rhs}];
}

// Carries enumerator names (lowercased) and their integer representation
// values for an enumeration type. Used as the type_info payload of TypeOp.
def EnumTypeInfoAttr : AttrDef<Ada_Dialect, "EnumTypeInfo"> {
  let mnemonic = "enum_info";
  let summary = "Enumeration type metadata (enumerator names and rep values)";
  let parameters = (ins
    StringRefArrayParameter:$names,
    ArrayRefParameter<"int64_t">:$values
  );
  let hasCustomAssemblyFormat = 1;
}

// Carries scalar type metadata for non-enumeration Ada types: universal_integer,
// universal_real, and future concrete integer/float types.
// Used as the type_info payload of TypeOp.
def ScalarTypeInfoAttr : AttrDef<Ada_Dialect, "ScalarTypeInfo"> {
  let mnemonic = "scalar_info";
  let summary = "Scalar type metadata (encoding and bit width)";
  let parameters = (ins
    EnumParameter<Ada_ScalarEncoding>:$encoding,
    "uint64_t":$bitWidth
  );
  let assemblyFormat = "`<` $encoding `,` $bitWidth `>`";
}

#endif // ADA_ATTRS
