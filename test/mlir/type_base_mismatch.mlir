// `ada.type` cross-checks a `base` link against the referenced type when it
// resolves: a subtype shares its base type's representation.

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: error: 'ada.type' op base type 'integer' has different mlir_type

module {
  ada.type @integer : i32 = #ada.int_info
  ada.type @small base @integer : i8 = #ada.int_info
}
