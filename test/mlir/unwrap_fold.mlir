// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// At -O1, an `ada.unwrap` of an `ada.wrap` folds to the wrapped value.

// RUN: %lalvm -O0 --emit=mlir %s | %FileCheck %s --check-prefix=O0
// RUN: %lalvm -O1 --emit=mlir %s | %FileCheck %s --check-prefix=O1

module {
  // O0-LABEL: ada.subp @roundtrip
  // O0:         %[[W:.*]] = ada.wrap %arg0 : i16 to !ada.qual<i16, @short_integer>
  // O0-NEXT:    %[[U:.*]] = ada.unwrap %[[W]] : !ada.qual<i16, @short_integer> to i16
  // O0-NEXT:    ada.return %[[U]] : i16
  // O1-LABEL: ada.subp @roundtrip
  // O1-NEXT:    ada.return %arg0 : i16
  ada.subp @roundtrip(%v: i16) -> i16 {
    %w = ada.wrap %v : i16 to !ada.qual<i16, @short_integer>
    %u = ada.unwrap %w : !ada.qual<i16, @short_integer> to i16
    ada.return %u : i16
  }
}
