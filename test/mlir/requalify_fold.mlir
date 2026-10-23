// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// At -O1, an `ada.requalify` to its own type folds to its value.

// RUN: %lalvm -O1 --emit=mlir %s | %FileCheck %s

module {
  // CHECK-LABEL: ada.subp @identity
  // CHECK-NOT:     ada.requalify
  // CHECK:         ada.return %arg0 : !ada.qual<i32, @integer>
  ada.subp @identity(%v: !ada.qual<i32, @integer>) -> !ada.qual<i32, @integer> {
    %r = ada.requalify %v : !ada.qual<i32, @integer> to !ada.qual<i32, @integer>
    ada.return %r : !ada.qual<i32, @integer>
  }
}
