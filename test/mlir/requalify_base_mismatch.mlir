// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// `ada.requalify` stays within one type: a different base is rejected.

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: error: 'ada.requalify' op value type @other and result type @positive do not share a base type

module {
  ada.type @integer : i32 = #ada.int_info<range -2147483648 to 2147483647>
  ada.type @other : i32 = #ada.int_info<range -100 to 100>
  ada.type @positive base @integer : i32 = #ada.int_info<range 1 to 2147483647>
  ada.subp @bad(%v: !ada.qual<i32, @other>) -> !ada.qual<i32, @positive> {
    %r = ada.requalify %v : !ada.qual<i32, @other> to !ada.qual<i32, @positive>
    ada.return %r : !ada.qual<i32, @positive>
  }
}
