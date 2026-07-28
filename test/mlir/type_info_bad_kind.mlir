// `type_info` must be one of the Ada info attributes (`enum_info`,
// `int_info`, `float_info`); any other attribute kind is rejected.

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: error: 'ada.type' op unsupported type_info attribute kind

module {
  ada.type @bad : i32 = 42
}
