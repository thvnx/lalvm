// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// `ada.unop` operand and result must belong to one type.

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: error: 'ada.unop' op operand and result types @count and @integer do not share a base type

module {
  ada.type @integer : i32 = #ada.int_info<range -2147483648 to 2147483647>
  ada.type @count : i32 = #ada.int_info<range 0 to 100>
  ada.subp @bad(%a: !ada.qual<i32, @count>) -> !ada.qual<i32, @integer> {
    %0 = ada.unop "-" %a : (!ada.qual<i32, @count>) -> !ada.qual<i32, @integer>
    ada.return %0 : !ada.qual<i32, @integer>
  }
}
