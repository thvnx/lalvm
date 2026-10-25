// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// The component check resolves a type declared in an enclosing subprogram's
// `ada.decls`, which `lookupNearestSymbolFrom` cannot reach.

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: error: {{.*}}array component 'outer.comp' has mlir_type 'i32', expected 'f32'

module {
  ada.type @integer : i32 = #ada.int_info<range -2147483648 to 2147483647>
  ada.subp @outer() {
    ada.decls {
      ada.type @outer.comp : i32 = #ada.int_info<range 0 to 9>
      ada.subp private @outer.inner() {
        ada.decls {
          ada.type @outer.inner.bad : !ada.array<f32[i32 x 10]> = #ada.array_info<component @outer.comp, dim @integer range 1 to 10>
        }
        ada.return
      }
    }
    ada.return
  }
}
