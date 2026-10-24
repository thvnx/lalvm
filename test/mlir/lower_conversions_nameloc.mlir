// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// Only the op yielding a named converted value keeps the object name.
// The bounds and descriptor of its check do not. A cast keeps it with the
// target's Ada type, the unwrap and wrap around it do not.

// RUN: %lalvm --emit=llvm --mlir-print-ir-after=ada-lower-conversions --mlir-print-debuginfo --mlir-print-local-scope %s -o /dev/null 2>&1 | %FileCheck %s

// CHECK-LABEL: ada.subp @named
// CHECK:         ada.constant : !ada.qual<i32, @integer> = 1 loc("f.adb":3:5)
// CHECK:         ada.constant : !ada.qual<i32, @integer> = 10 loc("f.adb":3:5)
// CHECK:         ada.range {{.*}} loc("f.adb":3:5)
// CHECK:         ada.range_check {{.*}} loc("x"("f.adb":3:5))

// CHECK-LABEL: ada.subp @named_widening
// CHECK:         ada.unwrap {{.*}} loc("f.adb":4:5)
// CHECK-NEXT:    arith.extsi {{.*}} loc("y"(fused<#ada.typeref<@integer>>["f.adb":4:5]))
// CHECK-NEXT:    ada.wrap {{.*}} loc("f.adb":4:5)

module {
  ada.type @integer : i32 = #ada.int_info<range -2147483648 to 2147483647>
  ada.type @small base @integer : i32 = #ada.int_info<range 1 to 10>
  ada.type @short : i16 = #ada.int_info<range -32768 to 32767>
  ada.subp @named(%v: !ada.qual<i32, @integer>) -> !ada.qual<i32, @small> {
    %c = ada.convert implicit %v : !ada.qual<i32, @integer> to !ada.qual<i32, @small> loc("x"("f.adb":3:5))
    ada.return %c : !ada.qual<i32, @small>
  }
  ada.subp @named_widening(%v: !ada.qual<i16, @short>) -> !ada.qual<i32, @integer> {
    %c = ada.convert explicit %v : !ada.qual<i16, @short> to !ada.qual<i32, @integer> loc("y"("f.adb":4:5))
    ada.return %c : !ada.qual<i32, @integer>
  }
}
