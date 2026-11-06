// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// Round trip of `ada.index_check`, from the base type to the index subtype.

// RUN: %lalvm --emit=mlir %s | %FileCheck %s
// RUN: %lalvm --emit=mlir %s | %lalvm -x mlir --emit=mlir - | %FileCheck %s
// RUN: %lalvm --emit=mlir -O1 %s | %FileCheck %s

module {
  ada.type @integer : i32 = #ada.int_info<range -2147483648 to 2147483647>
  ada.type @vec_index base @integer : i32 = #ada.int_info<range 1 to 10>
  // CHECK-LABEL: ada.subp @checks
  ada.subp @checks(%i: !ada.qual<i32, @integer>) -> !ada.qual<i32, @vec_index> {
    %lo = ada.constant : !ada.qual<i32, @integer> = 1
    %hi = ada.constant : !ada.qual<i32, @integer> = 10
    // CHECK: %[[R:.*]] = ada.range
    %r = ada.range %lo, %hi : !ada.qual<i32, @integer> -> !ada.range<i32, @vec_index>
    // CHECK: %[[CHECKED:.*]] = ada.index_check %arg0, %[[R]] : !ada.qual<i32, @integer> to !ada.qual<i32, @vec_index>
    %c = ada.index_check %i, %r : !ada.qual<i32, @integer> to !ada.qual<i32, @vec_index>
    // CHECK-NEXT: ada.return %[[CHECKED]] : !ada.qual<i32, @vec_index>
    ada.return %c : !ada.qual<i32, @vec_index>
  }
}
