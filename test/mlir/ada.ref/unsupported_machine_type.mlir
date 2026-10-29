// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: ada.ref requires {{.*}}ada.qual value type

module {
  ada.type @integer : i32 = #ada.int_info<range -2147483648 to 2147483647>
  ada.subp @bad(%x: !ada.ref<!ada.qual<index, @integer>>) { ada.return }
}
