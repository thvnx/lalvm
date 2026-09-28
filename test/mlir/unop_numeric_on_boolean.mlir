// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// `ada.unop` `-` operates on numeric types; a Boolean (`i1`) operand is
// rejected.

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: error: 'ada.unop' op unsupported operand type 'i1'; expected integer or float

module {
  ada.subp @bad(%a: !ada.qual<i1, @s>) -> !ada.qual<i1, @s> {
    %0 = ada.unop "-" %a : !ada.qual<i1, @s>
    ada.return %0 : !ada.qual<i1, @s>
  }
}
