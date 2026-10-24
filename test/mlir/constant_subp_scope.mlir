// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// At -O1 (and above), check that constants stay in their own subprogram: none
// is hoisted to the module or shared with a nested subprogram.

// RUN: %lalvm -O1 --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
// RUN: %lalvm -O1 --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

// MLIR-NOT:     ada.constant
// MLIR-LABEL: ada.subp @outer
// MLIR:         %[[C:.*]] = ada.constant : !ada.qual<i32, @integer> = 42
// MLIR:         ada.decls {
// MLIR:           ada.subp private @outer.inner
// MLIR-NEXT:        %[[D:.*]] = ada.constant : !ada.qual<i32, @integer> = 42
// MLIR-NEXT:        ada.return %[[D]]
// MLIR:         ada.binop "+" %[[C]]

// LLVM-LABEL: define i32 @_ada_outer(
// LLVM:         call i32 @outer__inner()
// LLVM-LABEL: define internal i32 @outer__inner(
// LLVM:         ret i32 42

module {
  ada.type @integer : i32 = #ada.int_info<range -2147483648 to 2147483647>
  ada.subp @outer() -> !ada.qual<i32, @integer> {
    %c = ada.constant : !ada.qual<i32, @integer> = 42
    ada.decls {
      ada.subp private @outer.inner() -> !ada.qual<i32, @integer> {
        %d = ada.constant : !ada.qual<i32, @integer> = 42
        ada.return %d : !ada.qual<i32, @integer>
      }
    }
    %r = ada.call @outer.inner() : () -> !ada.qual<i32, @integer>
    %s = ada.binop "+" %c, %r : (!ada.qual<i32, @integer>, !ada.qual<i32, @integer>) -> !ada.qual<i32, @integer>
    ada.return %s : !ada.qual<i32, @integer>
  }
}
