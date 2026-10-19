// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// `ada.type` rejects a `base` symbol that is not an `ada.type`.

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: error: 'ada.type' op base symbol 'proc' is not an ada.type

module {
  ada.subp @proc() {
    ada.return
  }
  ada.type @small base @proc : i8 = #ada.int_info
}
