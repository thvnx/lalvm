// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// RUN: %not %lalvm --emit=llvm %s 2>&1 | %FileCheck %s
// CHECK: failed to legalize operation 'arith.select'

module {
  ada.type @integer : i32 = #ada.int_info<range -2147483648 to 2147483647>
  ada.subp @bad(%condition: i1) {
    %x = ada.object : !ada.ref<!ada.qual<i32, @integer>>
    %y = ada.object : !ada.ref<!ada.qual<i32, @integer>>
    %selected = arith.select %condition, %x, %y : !ada.ref<!ada.qual<i32, @integer>>
    ada.return
  }
}
