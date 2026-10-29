// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s
// CHECK: result component 'f32' does not match array component 'i32'

module {
  ada.subp @bad(%offset: !ada.qual<i32, @integer>) {
    %array = ada.alloca : memref<!ada.qual<!ada.array<i32[i32 x 10]>, @array>>
    %element = ada.index %array[%offset] : (memref<!ada.qual<!ada.array<i32[i32 x 10]>, @array>>, !ada.qual<i32, @integer>) -> !ada.ref<!ada.qual<f32, @float>>
    ada.return
  }
}
