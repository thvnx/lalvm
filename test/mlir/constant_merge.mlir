// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// At -O1, identical constants merge. Same value of another Ada type does not.

// RUN: %lalvm -O1 --emit=mlir %s | %FileCheck %s

module {
  // CHECK-LABEL: ada.subp @merge
  // CHECK-DAG:     %[[I:.*]] = ada.constant : !ada.qual<i32, @integer> = 42
  // CHECK-DAG:     %[[P:.*]] = ada.constant : !ada.qual<i32, @positive> = 42
  // CHECK-NOT:     ada.constant
  // CHECK:         ada.requalify %[[P]]
  // CHECK:         ada.binop "+" %[[I]], %[[I]]
  ada.subp @merge() -> !ada.qual<i32, @integer> {
    %a = ada.constant : !ada.qual<i32, @integer> = 42
    %b = ada.constant : !ada.qual<i32, @integer> = 42
    %p = ada.constant : !ada.qual<i32, @positive> = 42
    %q = ada.requalify %p : !ada.qual<i32, @positive> to !ada.qual<i32, @integer>
    %s = ada.binop "+" %a, %b : !ada.qual<i32, @integer>
    %t = ada.binop "+" %s, %q : !ada.qual<i32, @integer>
    ada.return %t : !ada.qual<i32, @integer>
  }
}
