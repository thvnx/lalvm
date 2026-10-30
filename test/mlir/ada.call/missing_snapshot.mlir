// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s
// CHECK: 'ada.call' op missing or unexpected writable formal snapshot

module {
  ada.type @integer : i32 = #ada.int_info<range -2147483648 to 2147483647>
  ada.subp @update(%x: !ada.ref<!ada.qual<i32, @integer>> {ada.mode = #ada<mode in_out>}) {
    ada.return
  }
  ada.subp @caller() {
    %x = ada.object : !ada.ref<!ada.qual<i32, @integer>>
    ada.call @update(%x) : (!ada.ref<!ada.qual<i32, @integer>>) -> ()
    ada.return
  }
}
