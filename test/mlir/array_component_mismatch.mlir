// An array_info component symbol that resolves to a type whose machine type
// disagrees with the array element is rejected by TypeOp's verifier: `@comp` is
// `i32`, but the array element is `f32`.

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: error: {{.*}}array component

module {
  ada.type @comp : i32 = #ada.int_info<range -2147483648 to 2147483647>

  ada.type @bad : !ada.array<f32[i32 x 10]> = #ada.array_info<component @comp, dim @comp range 1 to 10>
}
