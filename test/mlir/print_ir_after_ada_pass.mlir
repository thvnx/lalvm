// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// Check that --mlir-print-ir-after accepts Ada passes by their names.

// RUN: %lalvm --emit=llvm --mlir-print-ir-after=ada-closure-conversion %s -o /dev/null 2>&1 | %FileCheck %s

// CHECK: IR Dump After ClosureConversion (ada-closure-conversion)

module {
  ada.subp @empty() {
    ada.return
  }
}
