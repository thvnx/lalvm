// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// The value and result of `ada.range_check` belong to one type: a value whose
// base differs from the result subtype's is rejected.

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: error: 'ada.range_check' op value type @other and result type @positive do not share a base type

module {
  ada.type @integer : i32 = #ada.int_info<range -2147483648 to 2147483647>
  ada.type @other : i32 = #ada.int_info<range -100 to 100>
  ada.type @positive base @integer : i32 = #ada.int_info<range 1 to 2147483647>
  ada.subp @bad(%v: !ada.qual<i32, @other>) {
    %lo = ada.constant : !ada.qual<i32, @integer> = 1
    %hi = ada.constant : !ada.qual<i32, @integer> = 100
    %r = ada.range %lo, %hi : !ada.qual<i32, @integer> -> !ada.range<i32, @positive>
    %c = ada.range_check %v, %r : !ada.qual<i32, @other> to !ada.qual<i32, @positive>
    ada.return
  }
}
