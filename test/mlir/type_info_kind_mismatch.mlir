// The `type_info` kind must match the machine type: an `int_info` on a float
// `mlir_type` is rejected by `TypeOp::verify`.

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: error: 'ada.type' op type_info kind does not match mlir_type

module {
  ada.type @bad : f32 = #ada.int_info<mod 256>
}
