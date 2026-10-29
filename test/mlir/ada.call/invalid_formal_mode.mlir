// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: ada.mode must be an Ada parameter mode
module {
  ada.subp @bad(%x: i32 {ada.mode = "out"}) { ada.return }
}
