// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// `UnOp::parse` rejects unsupported operators via `symbolizeAdaUnaryOp`.

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: error: custom op 'ada.unop' unknown unary operator '_'

module {
  ada.subp @bad(%a: i1) -> i1 {
    %0 = ada.unop "_" %a : i1
    ada.return %0 : i1
  }
}
