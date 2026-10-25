// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// The base link check resolves a type declared in an enclosing subprogram's
// `ada.decls`, which `lookupNearestSymbolFrom` cannot reach.

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: error: 'ada.type' op base type 'outer.t' has different mlir_type

module {
  ada.subp @outer() {
    ada.decls {
      ada.type @outer.t : i32 = #ada.int_info<range 0 to 9>
      ada.subp private @outer.inner() {
        ada.decls {
          ada.type @outer.inner.s base @outer.t : i8 = #ada.int_info<range 0 to 9>
        }
        ada.return
      }
    }
    ada.return
  }
}
