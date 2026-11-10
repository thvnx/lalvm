// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// RUN: %lalvm --emit=llvm --mlir-print-ir-after=ada-hoist-nested-symbols %s -o /dev/null 2>&1 | %FileCheck %s

// Existing module types and hoisted types form one group in walk order.
// CHECK-LABEL: module {
// CHECK-NEXT: ada.type @integer
// CHECK-NEXT: ada.type @first.local
// CHECK-NEXT: ada.type @late
// CHECK-NEXT: ada.type @second.local
// CHECK-NEXT: ada.subp @_ada_first
// CHECK-NOT: ada.type
// CHECK: ada.subp @_ada_second
// CHECK-NOT: ada.type
// CHECK-NOT: ada.decls

module {
  ada.type @integer : i32 = #ada.int_info<range -2147483648 to 2147483647>
  ada.subp @first() {
    ada.decls {
      ada.type @first.local base @integer : i32
    }
    ada.return
  }
  ada.type @late : i8 = #ada.int_info<range -128 to 127>
  ada.subp @second() {
    ada.decls {
      ada.type @second.local base @integer : i32
    }
    ada.return
  }
}
