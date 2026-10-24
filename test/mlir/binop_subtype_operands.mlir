// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// `ada.binop` operands may have different subtypes of its result type.

// RUN: %lalvm --emit=mlir %s | %FileCheck %s

// CHECK: ada.binop "+" %arg0, %arg1 checks<overflow> : (!ada.qual<i32, @positive>, !ada.qual<i32, @natural>) -> !ada.qual<i32, @integer>

module {
  ada.type @integer : i32 = #ada.int_info<range -2147483648 to 2147483647>
  ada.type @natural base @integer : i32 = #ada.int_info<range 0 to 2147483647>
  ada.type @positive base @integer : i32 = #ada.int_info<range 1 to 2147483647>
  ada.subp @add(%a: !ada.qual<i32, @positive>, %b: !ada.qual<i32, @natural>) -> !ada.qual<i32, @integer> {
    %0 = ada.binop "+" %a, %b checks<overflow> : (!ada.qual<i32, @positive>, !ada.qual<i32, @natural>) -> !ada.qual<i32, @integer>
    ada.return %0 : !ada.qual<i32, @integer>
  }
}
