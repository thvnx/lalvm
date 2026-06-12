// An `ada.type` may omit `type_info` only when it has a `base` to inherit
// from (an unconstrained subtype); a standalone type must carry its own.

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: error: 'ada.type' op type without a base must carry a type_info attribute

module {
  ada.type @orphan : i32
}
