// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// RUN: %lalvm --emit=mlir %s | %lalvm -x mlir --emit=mlir - | %FileCheck %s
// RUN: %lalvm --emit=mlir -O1 %s | %FileCheck %s

// Array objects retain their identity during scalar promotion.

module {
  ada.type @integer : i32 = #ada.int_info<range -2147483648 to 2147483647>
  ada.type @vector : !ada.array<i32[i32 x 3]> = #ada.array_info<component @integer, dim @integer range 1 to 3>
  ada.subp @array_object(%i: !ada.qual<i32, @integer>) {
    // CHECK: %[[ARRAY:.*]] = ada.object : !ada.ref<!ada.qual<!ada.array<i32[i32 x 3]>, @vector>>
    %array = ada.object : !ada.ref<!ada.qual<!ada.array<i32[i32 x 3]>, @vector>>
    // CHECK: ada.index %[[ARRAY]][%arg0] : (!ada.ref<!ada.qual<!ada.array<i32[i32 x 3]>, @vector>>, !ada.qual<i32, @integer>) -> !ada.ref<!ada.qual<i32, @integer>>
    %element = ada.index %array[%i] : (!ada.ref<!ada.qual<!ada.array<i32[i32 x 3]>, @vector>>, !ada.qual<i32, @integer>) -> !ada.ref<!ada.qual<i32, @integer>>
    %one = ada.constant : !ada.qual<i32, @integer> = 1
    ada.write %one, %element : !ada.qual<i32, @integer> -> !ada.ref<!ada.qual<i32, @integer>>
    ada.return
  }
}
