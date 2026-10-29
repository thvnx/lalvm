// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// RUN: %not %lalvm --emit=llvm %s 2>&1 | %FileCheck %s
// CHECK: failed to legalize operation 'ada.object'

module {
  ada.subp @bad() {
    %r = ada.object : !ada.ref<!ada.qual<i32, @missing>>
    ada.return
  }
}
