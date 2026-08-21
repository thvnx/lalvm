// An array_info whose bounds do not imply the type's extent is rejected by
// TypeOp's verifier (extent 10 vs a 1..5 range, which implies 5).

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: error: {{.*}}bounds imply extent

module {
  ada.type @bad : !ada.array<i32[i32 x 10]> = #ada.array_info<component @standard.integer, dim @standard.integer range 1 to 5>
}
