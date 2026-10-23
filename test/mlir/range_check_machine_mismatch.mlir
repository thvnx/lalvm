// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// `ada.range_check` forwards the machine value unchanged: a result of another
// machine type is rejected.

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: error: 'ada.range_check' op value machine type 'i64' does not match result machine type 'i32'

module {
  ada.subp @bad(%v: !ada.qual<i64, @long_integer>) {
    %lo = ada.constant : !ada.qual<i32, @integer> = 1
    %hi = ada.constant : !ada.qual<i32, @integer> = 100
    %r = ada.range %lo, %hi : !ada.qual<i32, @integer> -> !ada.range<i32, @positive>
    %c = ada.range_check %v, %r : !ada.qual<i64, @long_integer> to !ada.qual<i32, @positive>
    ada.return
  }
}
