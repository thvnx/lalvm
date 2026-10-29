// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: capture arguments must follow source formals
module {
  ada.subp @bad(%captured: i32 {ada.capture}, %formal: i32) { ada.return }
}
