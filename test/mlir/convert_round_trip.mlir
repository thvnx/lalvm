// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// Round-trip coverage for the `ada.convert` op.

// RUN: %lalvm --emit=mlir %s | %FileCheck %s

module {
  // CHECK-LABEL: ada.subp @convert
  ada.subp @convert(%i: !ada.qual<i16, @short_integer>, %f: !ada.qual<f32, @float>) {
    // CHECK: ada.convert explicit %arg0 : !ada.qual<i16, @short_integer> to !ada.qual<i32, @integer>
    %0 = ada.convert explicit %i : !ada.qual<i16, @short_integer> to !ada.qual<i32, @integer>
    // CHECK: ada.convert implicit %arg0 : !ada.qual<i16, @short_integer> to !ada.qual<i16, @small>
    %1 = ada.convert implicit %i : !ada.qual<i16, @short_integer> to !ada.qual<i16, @small>
    // CHECK: ada.convert implicit %arg1 : !ada.qual<f32, @float> to !ada.qual<f64, @long_float>
    %2 = ada.convert implicit %f : !ada.qual<f32, @float> to !ada.qual<f64, @long_float>
    // CHECK: ada.convert explicit %arg1 : !ada.qual<f32, @float> to !ada.qual<i32, @integer>
    %3 = ada.convert explicit %f : !ada.qual<f32, @float> to !ada.qual<i32, @integer>
    // A dynamic target subtype carries its range descriptor.
    // CHECK: %[[R:.*]] = ada.range
    // CHECK: ada.convert implicit %{{.*}}, %[[R]] : !ada.qual<i32, @integer> to !ada.qual<i32, @dyn>
    %lo = ada.constant : !ada.qual<i32, @integer> = 1
    %r = ada.range %lo, %3 : !ada.qual<i32, @integer> -> !ada.range<i32, @dyn>
    %4 = ada.convert implicit %3, %r : !ada.qual<i32, @integer> to !ada.qual<i32, @dyn>
    ada.return
  }
}
