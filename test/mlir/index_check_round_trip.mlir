// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// Round trip of `ada.index_check`, from the base type to the index subtype.

// RUN: %lalvm --emit=mlir %s | %FileCheck %s

module {
  // CHECK-LABEL: ada.subp @checks
  ada.subp @checks(%i: !ada.qual<i32, @integer>) {
    %lo = ada.constant : !ada.qual<i32, @integer> = 1
    %hi = ada.constant : !ada.qual<i32, @integer> = 10
    // CHECK: %[[R:.*]] = ada.range
    %r = ada.range %lo, %hi : !ada.qual<i32, @integer> -> !ada.range<i32, @vec_index>
    // CHECK: ada.index_check %arg0, %[[R]] : !ada.qual<i32, @integer> to !ada.qual<i32, @vec_index>
    %c = ada.index_check %i, %r : !ada.qual<i32, @integer> to !ada.qual<i32, @vec_index>
    ada.return
  }
}
