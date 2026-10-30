// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// REQUIRES: cc
// RUN: %lalvm --emit=mlir -O1 %s | %FileCheck %s --check-prefix=SEMANTIC
// SEMANTIC: ada.convert implicit
// RUN: %lalvm --emit=llvm --mlir-print-ir-after=ada-lower-calls %s -o /dev/null 2>&1 | %FileCheck %s --check-prefix=LOWER
// LOWER-LABEL: func.func @_ada_target(
// LOWER: %[[FORMAL:.*]] = ada.object : !ada.ref<!ada.qual<i32, @integer>> <- %arg0
// LOWER: ada.write %{{.*}}, %[[FORMAL]]
// LOWER-LABEL: func.func @_ada_verify(
// LOWER: %[[ACTUAL:.*]] = ada.object
// LOWER: %[[RESULT:.*]] = call @_ada_target(
// LOWER-NEXT: %[[VALUE:.*]] = ada.convert implicit %[[RESULT]]
// LOWER-NEXT: ada.write %[[VALUE]], %[[ACTUAL]]
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
// ERROR: raised CONSTRAINT_ERROR : body_failure.adb:1 range check failed

// The unused conversion must still raise at both optimization levels.
// IR checks keep the formal independent and assignment back after the call.
module {
  ada.type @integer : i32 = #ada.int_info<range -2147483648 to 2147483647>
  ada.type @small base @integer : i32 = #ada.int_info<range 1 to 5>
  ada.subp @target(%a: !ada.ref<!ada.qual<i32, @integer>> {ada.mode = #ada<mode in_out>}, %entered: !ada.ref<!ada.qual<i32, @integer>> {ada.capture}) {
    %one = ada.constant : !ada.qual<i32, @integer> = 1
    %nine = ada.constant : !ada.qual<i32, @integer> = 9
    %six = ada.constant : !ada.qual<i32, @integer> = 6
    ada.write %nine, %a : !ada.qual<i32, @integer> -> !ada.ref<!ada.qual<i32, @integer>>
    ada.write %one, %entered : !ada.qual<i32, @integer> -> !ada.ref<!ada.qual<i32, @integer>>
    %bad = ada.convert implicit %six : !ada.qual<i32, @integer> to !ada.qual<i32, @small> loc("body_failure.adb":1:1)
    ada.return
  }
  ada.subp @verify() -> !ada.qual<i32, @integer> {
    %zero = ada.constant : !ada.qual<i32, @integer> = 0
    %one = ada.constant : !ada.qual<i32, @integer> = 1
    %x = ada.object : !ada.ref<!ada.qual<i32, @integer>> <- %one : !ada.qual<i32, @integer>
    %entered = ada.object : !ada.ref<!ada.qual<i32, @integer>> <- %zero : !ada.qual<i32, @integer>
    ada.call @target(%x, %entered) initial_values(%one : !ada.qual<i32, @integer>) {ada.initial_formals = array<i32: 0>} : (!ada.ref<!ada.qual<i32, @integer>>, !ada.ref<!ada.qual<i32, @integer>>) -> ()
    ada.return %one : !ada.qual<i32, @integer>
  }
}
