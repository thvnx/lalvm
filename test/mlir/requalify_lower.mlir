// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// `ada.requalify` lowers to its operand: a chain leaves no instruction.

// RUN: %lalvm --emit=llvm %s | %FileCheck %s

module {
  ada.type @integer : i32 = #ada.int_info<range -2147483648 to 2147483647>
  ada.type @natural base @integer : i32 = #ada.int_info<range 0 to 2147483647>
  ada.type @positive base @integer : i32 = #ada.int_info<range 1 to 2147483647>

  // CHECK-LABEL: define i32 @_ada_chain(
  // CHECK-NEXT:    ret i32 %0
  ada.subp @chain(%v: !ada.qual<i32, @integer>) -> !ada.qual<i32, @positive> {
    %a = ada.requalify %v : !ada.qual<i32, @integer> to !ada.qual<i32, @natural>
    %b = ada.requalify %a : !ada.qual<i32, @natural> to !ada.qual<i32, @positive>
    ada.return %b : !ada.qual<i32, @positive>
  }
}
