// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s
// CHECK: 'ada.call' op ada.actual_order must be a permutation of source formals

module {
  ada.type @integer : i32 = #ada.int_info<range -2147483648 to 2147483647>
  ada.subp @callee(%a: !ada.qual<i32, @integer>, %b: !ada.qual<i32, @integer>) {
    ada.return
  }
  ada.subp @caller(%a: !ada.qual<i32, @integer>) {
    ada.call @callee(%a, %a) {ada.actual_order = array<i32: 1, 1>} : (!ada.qual<i32, @integer>, !ada.qual<i32, @integer>) -> ()
    ada.return
  }
}
