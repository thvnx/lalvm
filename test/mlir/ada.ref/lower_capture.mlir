// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// RUN: %lalvm --emit=mlir -O1 %s | %FileCheck %s --check-prefix=EARLY
// RUN: %lalvm --emit=llvm %s | %FileCheck %s
// RUN: %lalvm -O1 --emit=llvm --mlir-print-ir-after=ada-closure-conversion %s -o /dev/null 2>&1 | %FileCheck %s --check-prefix=CLOSURE
// RUN: %lalvm --emit=llvm -g %s | %FileCheck %s --check-prefix=DI
// RUN: %lalvm --emit=llvm --mlir-print-ir-after=ada-closure-conversion %s -o /dev/null 2>&1 | %FileCheck %s --check-prefix=CLOSURE

module {
  ada.type @integer : i32 = #ada.int_info<range -2147483648 to 2147483647>
  ada.type @small base @integer : i32 = #ada.int_info<range 1 to 10>

  // Early promotion must preserve an object used by a nested subprogram.
  // EARLY-LABEL: ada.subp @outer(
  // EARLY-SAME: %[[INPUT:[^: ]+]]: !ada.qual<i32, @small>
  // EARLY: %[[OBJECT:.*]] = ada.object : !ada.ref<!ada.qual<i32, @small>>
  // EARLY: ada.subp private @outer.inner()
  // EARLY: ada.write %[[INPUT]], %[[OBJECT]]
  // EARLY: ada.call @outer.inner()
  // EARLY: %[[VALUE:.*]] = ada.read %[[OBJECT]]
  // EARLY: ada.return %[[VALUE]]
  // CHECK-LABEL: define i32 @_ada_outer(
  // CHECK: %[[X:.*]] = alloca i32
  // CHECK: call void @outer__inner(i32 %0, ptr %[[X]])
  // CHECK: %[[VALUE:.*]] = load i32, ptr %[[X]]
  // CHECK: ret i32 %[[VALUE]]
  ada.subp @outer(%v: !ada.qual<i32, @small>) -> !ada.qual<i32, @integer> {
    %initial = ada.constant : !ada.qual<i32, @small> = 1
    %x = ada.object : !ada.ref<!ada.qual<i32, @small>> <- %initial : !ada.qual<i32, @small> loc("x"("capture.adb":1:1))
    ada.decls {
      ada.subp private @outer.inner() {
        ada.write %v, %x : !ada.qual<i32, @small> -> !ada.ref<!ada.qual<i32, @small>>
        ada.return
      }
    }
    ada.call @outer.inner() {arg_attrs = []} : () -> ()
    %value = ada.read %x : !ada.ref<!ada.qual<i32, @small>> -> !ada.qual<i32, @integer>
    ada.return %value : !ada.qual<i32, @integer>
  } loc("capture.adb":1:1)

  // CHECK-LABEL: define { i32, i32 } @_ada_formal(
  // CHECK: ret { i32, i32 }
  // DI-DAG: ![[SMALL:[0-9]+]] = !DISubrangeType(name: "small"
  // DI-DAG: DILocalVariable(name: "x", scope: {{.*}}, type: ![[SMALL]])
  // DI-DAG: DILocalVariable(name: "x", arg: 1, scope: {{.*}}, type: ![[SMALL]])
  ada.subp @formal(%x: !ada.ref<!ada.qual<i32, @small>> {ada.mode = #ada<mode in_out>} loc("x"("capture.adb":2:1))) -> !ada.qual<i32, @integer> {
    %value = ada.read %x : !ada.ref<!ada.qual<i32, @small>> -> !ada.qual<i32, @integer>
    ada.return %value : !ada.qual<i32, @integer>
  } loc("capture.adb":2:1)

  // CHECK-LABEL: define internal void @outer__inner(
  // CHECK: store i32 %0, ptr %1
  // CLOSURE-LABEL: ada.subp private @outer.inner(
  // CLOSURE-SAME: %arg0: !ada.qual<i32, @small> {ada.capture}
  // CLOSURE-SAME: %x: !ada.ref<!ada.qual<i32, @small>> {ada.capture}
  // CLOSURE-NEXT: ada.write %arg0, %x
}
