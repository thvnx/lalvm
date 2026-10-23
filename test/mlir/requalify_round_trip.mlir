// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// Round trip of `ada.requalify`.

// RUN: %lalvm --emit=mlir %s | %FileCheck %s

module {
  // CHECK-LABEL: ada.subp @requalify
  ada.subp @requalify(%v: !ada.qual<i32, @integer>) -> !ada.qual<i32, @count> {
    // CHECK: ada.requalify %arg0 : !ada.qual<i32, @integer> to !ada.qual<i32, @count>
    %r = ada.requalify %v : !ada.qual<i32, @integer> to !ada.qual<i32, @count>
    ada.return %r : !ada.qual<i32, @count>
  }
}
