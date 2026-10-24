// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// `ada.cmp` operands must share a machine type.

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: error: 'ada.cmp' op operand types '!ada.qual<i32, @integer>' and '!ada.qual<i16, @short>' do not share a machine type

module {
  ada.subp @bad(%a: !ada.qual<i32, @integer>, %b: !ada.qual<i16, @short>) -> !ada.qual<i1, @boolean> {
    %0 = ada.cmp "=" %a, %b : (!ada.qual<i32, @integer>, !ada.qual<i16, @short>) -> !ada.qual<i1, @boolean>
    ada.return %0 : !ada.qual<i1, @boolean>
  }
}
