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
include "Dialect.td"

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

#endif // ADA_ATTRS
