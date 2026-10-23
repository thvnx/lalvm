// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// Round-trip coverage for the `ada.range_check` op. It prints the value and
// result `!ada.qual` types, which may differ in their Ada type only; the
// `!ada.range` operand type is recovered from the result type on parse (the
// `TypesMatchWith` inference). The `ada.range` bounds are `ada.qual` values of
// the base type, printed ahead of the `!ada.range`.

// RUN: %lalvm --emit=mlir %s | %FileCheck %s

module {
  // CHECK-LABEL: ada.subp @checks
  ada.subp @checks(%v: !ada.qual<i32, @positive>, %n: !ada.qual<i32, @integer>) {
    // CHECK: %[[LO:.*]] = ada.constant : !ada.qual<i32, @integer> = 1
    %lo = ada.constant : !ada.qual<i32, @integer> = 1

    // Static bounds.
    // CHECK: %[[R:.*]] = ada.range %[[LO]], %{{.*}} : !ada.qual<i32, @integer> -> !ada.range<i32, @positive>
    %hi = ada.constant : !ada.qual<i32, @integer> = 100
    %r = ada.range %lo, %hi : !ada.qual<i32, @integer> -> !ada.range<i32, @positive>
    // CHECK: ada.range_check %arg0, %[[R]] : !ada.qual<i32, @positive> to !ada.qual<i32, @positive>
    %c = ada.range_check %v, %r : !ada.qual<i32, @positive> to !ada.qual<i32, @positive>

    // Dynamic upper bound: the check is the same op, the range is runtime.
    // CHECK: %[[RD:.*]] = ada.range %[[LO]], %arg1 : !ada.qual<i32, @integer> -> !ada.range<i32, @positive>
    %rd = ada.range %lo, %n : !ada.qual<i32, @integer> -> !ada.range<i32, @positive>
    // CHECK: ada.range_check %arg0, %[[RD]] : !ada.qual<i32, @positive> to !ada.qual<i32, @positive>
    %cd = ada.range_check %v, %rd : !ada.qual<i32, @positive> to !ada.qual<i32, @positive>

    // The check establishes the subtype on a value of the base type.
    // CHECK: ada.range_check %arg1, %[[R]] : !ada.qual<i32, @integer> to !ada.qual<i32, @positive>
    %cb = ada.range_check %n, %r : !ada.qual<i32, @integer> to !ada.qual<i32, @positive>

    ada.return
  }
}
