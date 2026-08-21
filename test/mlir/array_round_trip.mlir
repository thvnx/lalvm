// Round-trip a constrained array type through `ada.type`: the `!ada.array`
// layout in the type slot and its `array_info` metadata together. Each CHECK
// mirrors its input.

// RUN: %lalvm --emit=mlir %s | %FileCheck %s

module {
  ada.type @standard.integer : i32 = #ada.int_info<range -2147483648 to 2147483647>

  // 1-D static: array (1 .. 10) of Integer.
  // CHECK: ada.type @vec : !ada.array<i32[i32 x 10]> = #ada.array_info<component @standard.integer, dim @standard.integer range 1 to 10>
  ada.type @vec : !ada.array<i32[i32 x 10]> = #ada.array_info<component @standard.integer, dim @standard.integer range 1 to 10>

  // Multidimensional: array (1 .. 10, 1 .. 3) of Integer.
  // CHECK: ada.type @mat : !ada.array<i32[i32 x 10, i32 x 3]> = #ada.array_info<component @standard.integer, dim @standard.integer range 1 to 10, dim @standard.integer range 1 to 3>
  ada.type @mat : !ada.array<i32[i32 x 10, i32 x 3]> = #ada.array_info<component @standard.integer, dim @standard.integer range 1 to 10, dim @standard.integer range 1 to 3>

  // Dynamic upper bound: array (1 .. N) of Integer, so the extent is `?`.
  // CHECK: ada.type @dyn : !ada.array<i32[i32 x ?]> = #ada.array_info<component @standard.integer, dim @standard.integer range 1 to ?>
  ada.type @dyn : !ada.array<i32[i32 x ?]> = #ada.array_info<component @standard.integer, dim @standard.integer range 1 to ?>

  // Negative lower bound: array (-5 .. 5) of Integer, extent 5 - (-5) + 1 = 11.
  // CHECK: ada.type @neg : !ada.array<i32[i32 x 11]> = #ada.array_info<component @standard.integer, dim @standard.integer range -5 to 5>
  ada.type @neg : !ada.array<i32[i32 x 11]> = #ada.array_info<component @standard.integer, dim @standard.integer range -5 to 5>
}
