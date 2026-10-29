// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: value must have the object's nominal type

module {
  ada.type @integer : i32 = #ada.int_info<range -2147483648 to 2147483647>
  ada.type @small base @integer : i32 = #ada.int_info<range 1 to 10>
  ada.subp @bad(%r: !ada.ref<!ada.qual<i32, @small>> {ada.mode = #ada<mode out>}, %v: !ada.qual<i32, @integer>) {
    ada.write %v, %r : !ada.qual<i32, @integer> -> !ada.ref<!ada.qual<i32, @small>>
    ada.return
  }
}
