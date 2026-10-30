// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// REQUIRES: cc
// RUN: %lalvm --emit=llvm --mlir-print-ir-after=ada-lower-calls %s -o /dev/null 2>&1 | %FileCheck %s --check-prefix=ORDER
// ORDER-LABEL: func.func @_ada_verify(
// ORDER: %[[X:.*]] = ada.object
// ORDER: %[[Y:.*]] = ada.object
// ORDER: %[[Z:.*]] = ada.object
// ORDER: %[[RESULTS:.*]]:3 = call @_ada_target()
// ORDER-NEXT: %[[C:.*]] = ada.convert implicit %[[RESULTS]]#2
// ORDER-NEXT: ada.write %[[C]], %[[Z]]
// ORDER-NEXT: %[[B:.*]] = ada.convert implicit %[[RESULTS]]#1 : !ada.qual<i32, @integer> to !ada.qual<i32, @small>
// ORDER-NEXT: ada.write %[[B]], %[[Y]]
// ORDER-NEXT: %[[A:.*]] = ada.convert implicit %[[RESULTS]]#0
// ORDER-NEXT: ada.write %[[A]], %[[X]]
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
// ERROR: raised CONSTRAINT_ERROR : copyback_failure.adb:1 range check failed

// Assign back in association order. C is assigned before B's failing subtype
// check, and A's assignment follows that check.
module {
  ada.type @integer : i32 = #ada.int_info<range -2147483648 to 2147483647>
  ada.type @small base @integer : i32 = #ada.int_info<range 1 to 5>
  ada.subp @target(%a: !ada.ref<!ada.qual<i32, @integer>> {ada.mode = #ada<mode out>}, %b: !ada.ref<!ada.qual<i32, @integer>> {ada.mode = #ada<mode out>}, %c: !ada.ref<!ada.qual<i32, @integer>> {ada.mode = #ada<mode out>}) {
    %two = ada.constant : !ada.qual<i32, @integer> = 2
    %six = ada.constant : !ada.qual<i32, @integer> = 6
    %eight = ada.constant : !ada.qual<i32, @integer> = 8
    ada.write %two, %a : !ada.qual<i32, @integer> -> !ada.ref<!ada.qual<i32, @integer>>
    ada.write %six, %b : !ada.qual<i32, @integer> -> !ada.ref<!ada.qual<i32, @integer>>
    ada.write %eight, %c : !ada.qual<i32, @integer> -> !ada.ref<!ada.qual<i32, @integer>>
    ada.return
  }
  ada.subp @verify() -> !ada.qual<i32, @integer> {
    %zero = ada.constant : !ada.qual<i32, @integer> = 0
    %one = ada.constant : !ada.qual<i32, @integer> = 1
    %small_one = ada.constant : !ada.qual<i32, @small> = 1
    %x = ada.object : !ada.ref<!ada.qual<i32, @integer>> <- %zero : !ada.qual<i32, @integer>
    %y = ada.object : !ada.ref<!ada.qual<i32, @small>> <- %small_one : !ada.qual<i32, @small>
    %z = ada.object : !ada.ref<!ada.qual<i32, @integer>> <- %zero : !ada.qual<i32, @integer>
    ada.call @target(%x, %y, %z) {ada.actual_order = array<i32: 2, 1, 0>} : (!ada.ref<!ada.qual<i32, @integer>>, !ada.ref<!ada.qual<i32, @small>>, !ada.ref<!ada.qual<i32, @integer>>) -> () loc("copyback_failure.adb":1:1)
    ada.return %one : !ada.qual<i32, @integer>
  }
}
