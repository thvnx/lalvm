// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// RUN: %lalvm --emit=mlir -O1 %s | %FileCheck %s --check-prefix=MLIR
// RUN: %lalvm --emit=llvm -O1 %s | %FileCheck %s --check-prefix=LLVM

module {
  ada.type @integer : i32 = #ada.int_info<range -2147483648 to 2147483647>

  // Unchecked operations remain removable when their results are unused.
  // MLIR-LABEL: ada.subp @unchecked(
  // MLIR-NEXT: ada.return
  // LLVM-LABEL: define void @_ada_unchecked(
  // LLVM-NEXT: ret void
  ada.subp @unchecked(%a: !ada.qual<i32, @integer>, %b: !ada.qual<i32, @integer>) {
    %sum = ada.binop "+" %a, %b : (!ada.qual<i32, @integer>, !ada.qual<i32, @integer>) -> !ada.qual<i32, @integer>
    %negative = ada.unop "-" %a : (!ada.qual<i32, @integer>) -> !ada.qual<i32, @integer>
    ada.return
  }
}
