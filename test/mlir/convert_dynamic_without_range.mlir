// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// A target subtype with a dynamic bound needs a range operand.

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: error: 'ada.convert' op target subtype @s has a dynamic bound but no range operand

module {
  ada.type @integer : i32 = #ada.int_info<range -2147483648 to 2147483647>
  ada.type @s base @integer : i32 = #ada.int_info<range 1 to ?>
  ada.subp @bad(%v: !ada.qual<i32, @integer>) -> !ada.qual<i32, @s> {
    %c = ada.convert implicit %v : !ada.qual<i32, @integer> to !ada.qual<i32, @s>
    ada.return %c : !ada.qual<i32, @s>
  }
}
