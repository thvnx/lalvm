// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// RUN: %lalvm -g --emit=llvm %s 2>&1 | %FileCheck %s

// A float parameter (or variable) whose `ada.type` is missing gets no debug
// info (only possible with MLIR input). Neither does a reference parameter
// never read or written whose element type is unknown.

// CHECK-DAG: warning: missing ada.type for parameter 'x', skipping debug info
// CHECK-DAG: warning: missing ada.type for parameter 'y', skipping debug info
// CHECK-DAG: warning: no load or store for reference parameter 'z', skipping debug info
// CHECK-DAG: warning: missing ada.type for variable 'f', skipping debug info

module @no_ada_type {
  ada.subp @no_ada_type(
      %x: !ada.qual<f32, @float> loc("x"),
      %y: memref<!ada.qual<f32, @float>> {ada.mode = #ada<mode out>} loc("y"),
      %z: memref<!ada.qual<f32, @float>> {ada.mode = #ada<mode out>} loc("z")) {
    %0 = ada.alloca : memref<!ada.qual<f32, @float>> loc("f")
    memref.store %x, %y[] : memref<!ada.qual<f32, @float>>
    ada.return
  }
}
