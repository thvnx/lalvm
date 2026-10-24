// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// Round trip of `ada.wrap`.

// RUN: %lalvm --emit=mlir %s | %FileCheck %s

module {
  // CHECK-LABEL: ada.subp @wrap
  // CHECK:         ada.wrap %{{.*}} : i32 to !ada.qual<i32, @integer>
  ada.subp @wrap(%v: !ada.qual<i32, @integer>) -> !ada.qual<i32, @integer> {
    %u = ada.unwrap %v : !ada.qual<i32, @integer> to i32
    %w = ada.wrap %u : i32 to !ada.qual<i32, @integer>
    ada.return %w : !ada.qual<i32, @integer>
  }
}
