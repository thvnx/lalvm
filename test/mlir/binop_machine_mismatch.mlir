// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// `ada.binop` operands and result must share a machine type.

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: error: 'ada.binop' op operand and result types '!ada.qual<i16, @short>' and '!ada.qual<i32, @integer>' do not share a machine type

module {
  ada.subp @bad(%a: !ada.qual<i16, @short>, %b: !ada.qual<i16, @short>) -> !ada.qual<i32, @integer> {
    %0 = ada.binop "+" %a, %b : (!ada.qual<i16, @short>, !ada.qual<i16, @short>) -> !ada.qual<i32, @integer>
    ada.return %0 : !ada.qual<i32, @integer>
  }
}
