// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: 'ada.index' op array operand must identify an Ada object

module {
  ada.subp @bad(%array: i32, %index: !ada.qual<i32, @integer>) {
    %element = ada.index %array[%index] : (i32, !ada.qual<i32, @integer>) -> !ada.ref<!ada.qual<i32, @integer>>
    ada.return
  }
}
