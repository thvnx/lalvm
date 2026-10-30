// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// RUN: %lalvm --emit=llvm --mlir-print-ir-after=ada-lower-references %s -o /dev/null 2>&1 | %FileCheck %s --check-prefix=LOWER --implicit-check-not=!ada.ref --implicit-check-not=unrealized_conversion_cast
// RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM
// RUN: %lalvm --emit=llvm -O1 %s | %FileCheck %s --check-prefix=LLVM

module {
  ada.type @integer : i32 = #ada.int_info<range -2147483648 to 2147483647>

  // Formal objects remain independent even when their actuals alias.
  // LOWER-LABEL: func.func @_ada_set_and_read(
  // LOWER-SAME: %arg0: !ada.qual<i32, @integer>, %arg1: !ada.qual<i32, @integer>, %arg2: !ada.qual<i32, @integer>) -> (!ada.qual<i32, @integer>, !ada.qual<i32, @integer>, !ada.qual<i32, @integer>)
  // LOWER: %[[X:.*]] = ada.alloca
  // LOWER: %[[Y:.*]] = ada.alloca
  // LOWER: memref.store %arg2, %[[X]][]
  // LOWER: %[[VALUE:.*]] = memref.load %[[Y]][]
  // LOWER: return %[[VALUE]],
  // LLVM-LABEL: define { i32, i32, i32 } @_ada_set_and_read(i32
  // LLVM: ret { i32, i32, i32 }
  ada.subp @set_and_read(%x: !ada.ref<!ada.qual<i32, @integer>> {ada.mode = #ada<mode in_out>}, %y: !ada.ref<!ada.qual<i32, @integer>> {ada.mode = #ada<mode in_out>}, %v: !ada.qual<i32, @integer>) -> !ada.qual<i32, @integer> {
    ada.write %v, %x : !ada.qual<i32, @integer> -> !ada.ref<!ada.qual<i32, @integer>>
    %value = ada.read %y : !ada.ref<!ada.qual<i32, @integer>> -> !ada.qual<i32, @integer>
    ada.return %value : !ada.qual<i32, @integer>
  }

  // Both aliases and distinct actuals start with the same saved value.
  // LOWER-LABEL: func.func @_ada_verify
  // LOWER: call @_ada_set_and_read
  // LOWER: memref.store
  // LOWER: memref.store
  // LOWER: call @_ada_set_and_read
  // LLVM-LABEL: define i32 @_ada_verify(
  // LLVM: call { i32, i32, i32 } @_ada_set_and_read(i32 1, i32 1, i32 42)
  // LLVM: call { i32, i32, i32 } @_ada_set_and_read(i32
  ada.subp @verify() -> !ada.qual<i32, @integer> {
    %one = ada.constant : !ada.qual<i32, @integer> = 1
    %answer = ada.constant : !ada.qual<i32, @integer> = 42
    %x = ada.object : !ada.ref<!ada.qual<i32, @integer>> <- %one : !ada.qual<i32, @integer>
    %y = ada.object : !ada.ref<!ada.qual<i32, @integer>> <- %one : !ada.qual<i32, @integer>
    %aliased = ada.call @set_and_read(%x, %x, %answer) initial_values(%one, %one : !ada.qual<i32, @integer>, !ada.qual<i32, @integer>) {ada.initial_formals = array<i32: 0, 1>} : (!ada.ref<!ada.qual<i32, @integer>>, !ada.ref<!ada.qual<i32, @integer>>, !ada.qual<i32, @integer>) -> !ada.qual<i32, @integer>
    %saved_x = ada.read %x : !ada.ref<!ada.qual<i32, @integer>> -> !ada.qual<i32, @integer>
    %saved_y = ada.read %y : !ada.ref<!ada.qual<i32, @integer>> -> !ada.qual<i32, @integer>
    %separate = ada.call @set_and_read(%x, %y, %answer) initial_values(%saved_x, %saved_y : !ada.qual<i32, @integer>, !ada.qual<i32, @integer>) {ada.initial_formals = array<i32: 0, 1>} : (!ada.ref<!ada.qual<i32, @integer>>, !ada.ref<!ada.qual<i32, @integer>>, !ada.qual<i32, @integer>) -> !ada.qual<i32, @integer>
    ada.return %separate : !ada.qual<i32, @integer>
  }
}
