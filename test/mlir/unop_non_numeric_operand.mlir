// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// `ada.unop` operates on integers and floats only.

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: error: 'ada.unop' op unsupported operand type 'index'; expected integer or float

module {
  ada.subp @bad(%a: index) -> index {
    %0 = ada.unop "-" %a : index
    ada.return %0 : index
  }
}
