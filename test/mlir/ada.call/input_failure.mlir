// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// REQUIRES: cc
// RUN: %lalvm --emit=llvm --mlir-print-ir-after=ada-lower-conversions %s -o /dev/null 2>&1 | %FileCheck %s --check-prefix=ORDER
// ORDER-LABEL: func.func @_ada_verify(
// ORDER: ada.range_check
// ORDER-NEXT: %{{.*}} = call @_ada_later(
// ORDER-NEXT: %{{.*}} = call @_ada_target(
// RUN: echo 'function Verify return Integer is begin return 1; end Verify;' > %t.adb
// RUN: %lalvm --bind %t.adb -o %t.b.o
// RUN: %lalvm --emit=obj %s -o %t.o
// RUN: %cc %t.o %t.b.o %gnatstub -o %t
// RUN: %not %t 2> %t.err
// RUN: %FileCheck %s --check-prefix=ERROR < %t.err
// RUN: %lalvm --emit=obj -O1 %s -o %t.o
// RUN: %cc %t.o %t.b.o %gnatstub -o %t
// RUN: %not %t 2> %t.err
// RUN: %FileCheck %s --check-prefix=ERROR < %t.err
// ERROR: raised CONSTRAINT_ERROR : input_failure.adb:1 range check failed

// The input check must fail before the later actual or the callee body runs.
module {
  ada.type @integer : i32 = #ada.int_info<range -2147483648 to 2147483647>
  ada.type @small base @integer : i32 = #ada.int_info<range 1 to 5>
  ada.subp @later(%flag: !ada.ref<!ada.qual<i32, @integer>> {ada.capture}) -> !ada.qual<i32, @integer> {
    %one = ada.constant : !ada.qual<i32, @integer> = 1
    ada.write %one, %flag : !ada.qual<i32, @integer> -> !ada.ref<!ada.qual<i32, @integer>>
    ada.return %one : !ada.qual<i32, @integer>
  }
  ada.subp @target(%a: !ada.ref<!ada.qual<i32, @small>> {ada.mode = #ada<mode in_out>}, %b: !ada.qual<i32, @integer>, %entered: !ada.ref<!ada.qual<i32, @integer>> {ada.capture}) {
    %one = ada.constant : !ada.qual<i32, @integer> = 1
    ada.write %one, %entered : !ada.qual<i32, @integer> -> !ada.ref<!ada.qual<i32, @integer>>
    ada.return
  }
  ada.subp @verify() -> !ada.qual<i32, @integer> {
    %zero = ada.constant : !ada.qual<i32, @integer> = 0
    %one = ada.constant : !ada.qual<i32, @integer> = 1
    %seven = ada.constant : !ada.qual<i32, @integer> = 7
    %x = ada.object : !ada.ref<!ada.qual<i32, @integer>> <- %seven : !ada.qual<i32, @integer>
    %later = ada.object : !ada.ref<!ada.qual<i32, @integer>> <- %zero : !ada.qual<i32, @integer>
    %entered = ada.object : !ada.ref<!ada.qual<i32, @integer>> <- %zero : !ada.qual<i32, @integer>
    %value = ada.read %x : !ada.ref<!ada.qual<i32, @integer>> -> !ada.qual<i32, @integer>
    %checked = ada.convert implicit %value : !ada.qual<i32, @integer> to !ada.qual<i32, @small> loc("input_failure.adb":1:1)
    %b = ada.call @later(%later) : (!ada.ref<!ada.qual<i32, @integer>>) -> !ada.qual<i32, @integer>
    ada.call @target(%x, %b, %entered) initial_values(%checked : !ada.qual<i32, @small>) {ada.initial_formals = array<i32: 0>} : (!ada.ref<!ada.qual<i32, @integer>>, !ada.qual<i32, @integer>, !ada.ref<!ada.qual<i32, @integer>>) -> ()
    ada.return %one : !ada.qual<i32, @integer>
  }
}
