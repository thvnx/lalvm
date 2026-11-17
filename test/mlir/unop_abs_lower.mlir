// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// RUN: %lalvm --emit=llvm %s | %FileCheck %s

// Unchecked abs keeps the minimum signed value defined.
// CHECK-LABEL: define i32 @_ada_unchecked_abs(
// CHECK: %[[ABS:.*]] = call i32 @llvm.abs.i32(i32 %0, i1 false)
// CHECK-NEXT: ret i32 %[[ABS]]

module {
  ada.type @integer : i32 = #ada.int_info<range -2147483648 to 2147483647>
  ada.subp @unchecked_abs(%x: !ada.qual<i32, @integer>) -> !ada.qual<i32, @integer> {
    %abs = ada.unop "abs" %x : (!ada.qual<i32, @integer>) -> !ada.qual<i32, @integer>
    ada.return %abs : !ada.qual<i32, @integer>
  }
}
