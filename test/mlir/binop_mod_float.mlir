// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// Reject floating point operands for `mod` and `rem`.

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: error: 'ada.binop' op unsupported operand type 'f32'; expected integer

module {
  ada.subp @bad(%a: !ada.qual<f32, @s>, %b: !ada.qual<f32, @s>) -> !ada.qual<f32, @s> {
    %0 = ada.binop "mod" %a, %b : (!ada.qual<f32, @s>, !ada.qual<f32, @s>) -> !ada.qual<f32, @s>
    ada.return %0 : !ada.qual<f32, @s>
  }
}
