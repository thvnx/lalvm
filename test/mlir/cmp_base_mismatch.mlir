// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// `ada.cmp` operands must belong to one type.

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: error: 'ada.cmp' op requires operands to share a base type, got @integer and @count

module {
  ada.type @boolean : i1 = #ada.enum_info<"false" = 0, "true" = 1>
  ada.type @integer : i32 = #ada.int_info<range -2147483648 to 2147483647>
  ada.type @count : i32 = #ada.int_info<range 0 to 100>
  ada.subp @bad(%a: !ada.qual<i32, @integer>, %b: !ada.qual<i32, @count>) -> !ada.qual<i1, @boolean> {
    %0 = ada.cmp "=" %a, %b : (!ada.qual<i32, @integer>, !ada.qual<i32, @count>) -> !ada.qual<i1, @boolean>
    ada.return %0 : !ada.qual<i1, @boolean>
  }
}
