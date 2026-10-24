// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// No `ada.convert` is legal after LowerAdaConversions.

// RUN: %not %lalvm --emit=llvm %s 2>&1 | %FileCheck %s

// CHECK: error: failed to legalize operation 'ada.convert'

module {
  ada.type @integer : i32 = #ada.int_info<range -2147483648 to 2147483647>
  ada.subp @unresolved(%v: !ada.qual<i32, @integer>) -> !ada.qual<i32, @undeclared> {
    %r = ada.convert explicit %v : !ada.qual<i32, @integer> to !ada.qual<i32, @undeclared>
    ada.return %r : !ada.qual<i32, @undeclared>
  }
}
