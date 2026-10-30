// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// REQUIRES: cc
// RUN: %lalvm --emit=llvm --mlir-print-ir-after=ada-lower-references %s -o /dev/null 2>&1 | %FileCheck %s --check-prefix=LOWER --implicit-check-not=!ada.ref --implicit-check-not=unrealized_conversion_cast
// RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM
// RUN: %lalvm --emit=llvm -O1 %s | %FileCheck %s --check-prefix=LLVM
// RUN: echo 'function Verify return Integer is begin return 0; end Verify;' > %t.adb
// RUN: %lalvm --bind %t.adb -o %t.b.o
// RUN: %lalvm --emit=obj %s -o %t.o
// RUN: %cc %t.o %t.b.o -o %t
// RUN: %t
// RUN: %lalvm --emit=obj -O1 %s -o %t.o
// RUN: %cc %t.o %t.b.o -o %t
// RUN: %t

module {
  ada.type @integer : i32 = #ada.int_info<range -2147483648 to 2147483647>

  // An alias observes the write to the original object.
  // LOWER-LABEL: ada.subp @_ada_set_and_read(
  // LOWER-SAME: %arg0: memref<!ada.qual<i32, @integer>>
  // LOWER-SAME: %arg1: memref<!ada.qual<i32, @integer>>
  // LOWER: memref.store %arg2, %arg0[]
  // LOWER: %[[VALUE:.*]] = memref.load %arg1[]
  // LOWER: ada.return %[[VALUE]]
  // LLVM-LABEL: define i32 @_ada_set_and_read(
  // LLVM: store i32 %2, ptr %0
  // LLVM: %[[LOADED:.*]] = load i32, ptr %1
  // LLVM: ret i32 %[[LOADED]]
  ada.subp @set_and_read(%x: !ada.ref<!ada.qual<i32, @integer>> {ada.mode = #ada<mode in_out>}, %y: !ada.ref<!ada.qual<i32, @integer>> {ada.mode = #ada<mode in_out>}, %v: !ada.qual<i32, @integer>) -> !ada.qual<i32, @integer> {
    ada.write %v, %x : !ada.qual<i32, @integer> -> !ada.ref<!ada.qual<i32, @integer>>
    %value = ada.read %y : !ada.ref<!ada.qual<i32, @integer>> -> !ada.qual<i32, @integer>
    ada.return %value : !ada.qual<i32, @integer>
  }

  // The binder uses the temporary Ada unit's signature and this implementation.
  // Both aliases and distinct objects start with the same value.
  // LOWER-LABEL: ada.subp @_ada_verify
  // LOWER: %[[X:.*]] = ada.alloca
  // LOWER: %[[Y:.*]] = ada.alloca
  // LOWER: ada.call @_ada_set_and_read(%[[X]], %[[X]], %{{.*}})
  // LOWER: ada.call @_ada_set_and_read(%[[X]], %[[Y]], %{{.*}})
  // LLVM-LABEL: define i32 @_ada_verify(
  // LLVM: %[[X:.*]] = alloca i32
  // LLVM: %[[Y:.*]] = alloca i32
  // LLVM: call i32 @_ada_set_and_read(ptr %[[X]], ptr %[[X]], i32 42)
  // LLVM: call i32 @_ada_set_and_read(ptr %[[X]], ptr %[[Y]], i32 42)
  ada.subp @verify() -> !ada.qual<i32, @integer> {
    %one = ada.constant : !ada.qual<i32, @integer> = 1
    %answer = ada.constant : !ada.qual<i32, @integer> = 42
    %x = ada.object : !ada.ref<!ada.qual<i32, @integer>> <- %one : !ada.qual<i32, @integer>
    %y = ada.object : !ada.ref<!ada.qual<i32, @integer>> <- %one : !ada.qual<i32, @integer>
    %aliased = ada.call @set_and_read(%x, %x, %answer) : (!ada.ref<!ada.qual<i32, @integer>>, !ada.ref<!ada.qual<i32, @integer>>, !ada.qual<i32, @integer>) -> !ada.qual<i32, @integer>
    %separate = ada.call @set_and_read(%x, %y, %answer) : (!ada.ref<!ada.qual<i32, @integer>>, !ada.ref<!ada.qual<i32, @integer>>, !ada.qual<i32, @integer>) -> !ada.qual<i32, @integer>
    %alias_error = ada.binop "-" %aliased, %answer : (!ada.qual<i32, @integer>, !ada.qual<i32, @integer>) -> !ada.qual<i32, @integer>
    %distinct_error = ada.binop "-" %separate, %one : (!ada.qual<i32, @integer>, !ada.qual<i32, @integer>) -> !ada.qual<i32, @integer>
    %alias_squared = ada.binop "*" %alias_error, %alias_error : (!ada.qual<i32, @integer>, !ada.qual<i32, @integer>) -> !ada.qual<i32, @integer>
    %distinct_squared = ada.binop "*" %distinct_error, %distinct_error : (!ada.qual<i32, @integer>, !ada.qual<i32, @integer>) -> !ada.qual<i32, @integer>
    %result = ada.binop "+" %alias_squared, %distinct_squared : (!ada.qual<i32, @integer>, !ada.qual<i32, @integer>) -> !ada.qual<i32, @integer>
    ada.return %result : !ada.qual<i32, @integer>
  }
}
