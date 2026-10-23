// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// `ada.requalify` keeps the machine value: another machine type is rejected.

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: error: 'ada.requalify' op value machine type 'i64' does not match result machine type 'i32'

module {
  ada.subp @bad(%v: !ada.qual<i64, @long_integer>) -> !ada.qual<i32, @integer> {
    %r = ada.requalify %v : !ada.qual<i64, @long_integer> to !ada.qual<i32, @integer>
    ada.return %r : !ada.qual<i32, @integer>
  }
}
