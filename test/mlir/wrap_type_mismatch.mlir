// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// `ada.wrap` keeps the value's type.

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: error: 'ada.wrap' op operand type 'i16' must be the result's underlying type 'i32'

module {
  ada.subp @bad(%v: !ada.qual<i16, @short_integer>) -> !ada.qual<i32, @integer> {
    %u = ada.unwrap %v : !ada.qual<i16, @short_integer> to i16
    %w = ada.wrap %u : i16 to !ada.qual<i32, @integer>
    ada.return %w : !ada.qual<i32, @integer>
  }
}
