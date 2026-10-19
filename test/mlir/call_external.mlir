// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// `ada.call` accepts a callee absent from the module (defined in another unit).

// RUN: %lalvm --emit=mlir %s | %FileCheck %s

// CHECK: ada.call @other_unit.proc() : () -> ()

module {
  ada.subp @caller() {
    ada.call @other_unit.proc() : () -> ()
    ada.return
  }
}
