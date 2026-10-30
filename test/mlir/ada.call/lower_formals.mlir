// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// RUN: %lalvm --emit=llvm --mlir-print-ir-after=ada-lower-calls %s -o /dev/null 2>&1 | %FileCheck %s --check-prefix=CALLEE
// RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM
// RUN: %lalvm --emit=llvm -O1 %s | %FileCheck %s --check-prefix=LLVM
// RUN: %lalvm --emit=llvm -g %s | %FileCheck %s --check-prefix=DI

module {
  ada.type @integer : i32 = #ada.int_info<range -2147483648 to 2147483647>
  ada.type @small base @integer : i32 = #ada.int_info<range 1 to 10>
  ada.type @defaulted : i32 = #ada.int_info<range -2147483648 to 2147483647> {has_default_value}
  ada.type @defaulted_small base @defaulted : i32 = #ada.int_info<range 1 to 10>

  // Out has no input. In out receives a checked value and its own object.
  // CALLEE-LABEL: func.func @_ada_procedure(
  // CALLEE-SAME: %a: !ada.qual<i32, @small>, %arg1: !ada.qual<i32, @small>) -> (!ada.qual<i32, @integer>, !ada.qual<i32, @integer>)
  // CALLEE: %[[B:.*]] = ada.object : !ada.ref<!ada.qual<i32, @small>>
  // CALLEE: %[[C:.*]] = ada.object : !ada.ref<!ada.qual<i32, @small>> <- %arg1
  // CALLEE: ada.write %a, %[[B]]
  // CALLEE: %[[BVAL:.*]] = ada.read %[[B]] : !ada.ref<!ada.qual<i32, @small>> -> !ada.qual<i32, @integer>
  // CALLEE: %[[CVAL:.*]] = ada.read %[[C]] : !ada.ref<!ada.qual<i32, @small>> -> !ada.qual<i32, @integer>
  // CALLEE: return %[[BVAL]], %[[CVAL]] : !ada.qual<i32, @integer>, !ada.qual<i32, @integer>
  // LLVM-LABEL: define { i32, i32 } @_ada_procedure(i32
  // LLVM: ret { i32, i32 }
  // DI-DAG: ![[SMALL:[0-9]+]] = !DISubrangeType(name: "small"
  // DI-DAG: !{null, ![[SMALL]], ![[SMALL]], ![[SMALL]]}
  // DI-DAG: DILocalVariable(name: "b", arg: 2, scope: {{.*}}, type: ![[SMALL]])
  // DI-DAG: DILocalVariable(name: "c", arg: 3, scope: {{.*}}, type: ![[SMALL]])
  ada.subp @procedure(%a: !ada.qual<i32, @small> loc("a"("formals.adb":1:1)), %b: !ada.ref<!ada.qual<i32, @small>> {ada.mode = #ada<mode out>} loc("b"("formals.adb":1:1)), %c: !ada.ref<!ada.qual<i32, @small>> {ada.mode = #ada<mode in_out>} loc("c"("formals.adb":1:1))) attributes {ada.new_lowering} {
    ada.write %a, %b : !ada.qual<i32, @small> -> !ada.ref<!ada.qual<i32, @small>>
    ada.return
  } loc("formals.adb":1:1)

  // The required out input is copied without a nominal subtype conversion.
  // The Ada function result precedes the writable formal result.
  // CALLEE-LABEL: func.func @_ada_unchecked_out(%arg0: !ada.qual<i32, @defaulted>) -> (!ada.qual<i32, @defaulted>, !ada.qual<i32, @defaulted>)
  // CALLEE: %[[X:.*]] = ada.object : !ada.ref<!ada.qual<i32, @defaulted_small>> <- %arg0 : !ada.qual<i32, @defaulted>
  // CALLEE: %[[RESULT:.*]] = ada.read %[[X]]
  // CALLEE: %[[OUTPUT:.*]] = ada.read %[[X]]
  // CALLEE: return %[[RESULT]], %[[OUTPUT]]
  // LLVM-LABEL: define { i32, i32 } @_ada_unchecked_out(i32
  // LLVM: ret { i32, i32 }
  ada.subp @unchecked_out(%x: !ada.ref<!ada.qual<i32, @defaulted_small>> {ada.mode = #ada<mode out>} loc("x"("formals.adb":2:1))) -> !ada.qual<i32, @defaulted> attributes {ada.new_lowering} {
    %value = ada.read %x : !ada.ref<!ada.qual<i32, @defaulted_small>> -> !ada.qual<i32, @defaulted>
    ada.return %value : !ada.qual<i32, @defaulted>
  } loc("formals.adb":2:1)

  // Closure conversion hoists the callee and keeps its capture as a reference.
  // Only the source out formal contributes an ABI result.
  // CALLEE-LABEL: func.func private @outer__inner(%arg0: !ada.ref<!ada.qual<i32, @small>>) -> !ada.qual<i32, @integer>
  // CALLEE: %[[OUT:.*]] = ada.object : !ada.ref<!ada.qual<i32, @small>>
  // CALLEE: %[[CAPTURED:.*]] = ada.read %arg0
  // CALLEE: ada.write %[[CAPTURED]], %[[OUT]]
  // LLVM-LABEL: define internal i32 @outer__inner(ptr
  // LLVM: load i32, ptr %0
  // LLVM: ret i32
  ada.subp @outer(%v: !ada.qual<i32, @small>) {
    %r = ada.object : !ada.ref<!ada.qual<i32, @small>> <- %v : !ada.qual<i32, @small>
    ada.decls {
      ada.subp private @outer.inner(%x: !ada.ref<!ada.qual<i32, @small>> {ada.mode = #ada<mode out>}) attributes {ada.new_lowering} {
        %value = ada.read %r : !ada.ref<!ada.qual<i32, @small>> -> !ada.qual<i32, @small>
        ada.write %value, %x : !ada.qual<i32, @small> -> !ada.ref<!ada.qual<i32, @small>>
        ada.return
      }
    }
    ada.return
  }
}
