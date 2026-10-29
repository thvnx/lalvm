// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// Reference lowering is introduced separately; fail without crashing meanwhile.
// RUN: %not %lalvm --emit=llvm %s 2>&1 | %FileCheck %s
// CHECK: failed to legalize operation 'ada.subp'

module {
  ada.subp @pending(%r: !ada.ref<!ada.qual<i32, @integer>> {ada.mode = #ada<mode in_out>}) {
    ada.return
  }
}
