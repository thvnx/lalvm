// The extent and the bounds must agree on being static or dynamic. Here the
// upper bound is `?` (dynamic) but the extent is a concrete 10, so TypeOp's
// verifier rejects it.

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: error: {{.*}}dynamic

module {
  ada.type @bad : !ada.array<i32[i32 x 10]> = #ada.array_info<component @standard.integer, dim @standard.integer range 1 to ?>
}
