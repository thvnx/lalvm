// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// Generic syntax permits a mismatch between the range and result subtypes.
// CHECK: 'ada.index_check' op failed to verify that range matches the checked subtype

module {
  ada.type @integer : i32 = #ada.int_info<range -2147483648 to 2147483647>
  ada.type @vec_index base @integer : i32 = #ada.int_info<range 1 to 10>
  ada.subp @bad(%i: !ada.qual<i32, @integer>) {
    %lo = ada.constant : !ada.qual<i32, @integer> = 1
    %hi = ada.constant : !ada.qual<i32, @integer> = 10
    %r = ada.range %lo, %hi : !ada.qual<i32, @integer> -> !ada.range<i32, @vec_index>
    %c = "ada.index_check"(%i, %r) : (!ada.qual<i32, @integer>, !ada.range<i32, @vec_index>) -> !ada.qual<i32, @integer>
    ada.return
  }
}
