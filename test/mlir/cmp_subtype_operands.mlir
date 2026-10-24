// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// `ada.cmp` operands may have different subtypes of one type.

// RUN: %lalvm --emit=mlir %s | %FileCheck %s

// CHECK: ada.cmp "<" %arg0, %arg1 : (!ada.qual<i32, @small>, !ada.qual<i32, @integer>) -> !ada.qual<i1, @boolean>

module {
  ada.type @boolean : i1 = #ada.enum_info<"false" = 0, "true" = 1>
  ada.type @integer : i32 = #ada.int_info<range -2147483648 to 2147483647>
  ada.type @small base @integer : i32 = #ada.int_info<range 1 to 10>
  ada.subp @cmp(%a: !ada.qual<i32, @small>, %b: !ada.qual<i32, @integer>) -> !ada.qual<i1, @boolean> {
    %0 = ada.cmp "<" %a, %b : (!ada.qual<i32, @small>, !ada.qual<i32, @integer>) -> !ada.qual<i1, @boolean>
    ada.return %0 : !ada.qual<i1, @boolean>
  }
}
