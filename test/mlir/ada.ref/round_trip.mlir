// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// Objects retain distinct identities and writes remain observable under -O1.
// RUN: %lalvm --emit=mlir %s | %FileCheck %s
// RUN: %lalvm --emit=mlir -O1 %s | %FileCheck %s
// RUN: %lalvm --emit=mlir %s | %lalvm -x mlir --emit=mlir - | %FileCheck %s

module {
  ada.type @integer : i32 = #ada.int_info<range -2147483648 to 2147483647>
  ada.type @small base @integer : i32 = #ada.int_info<range 1 to 10>
  ada.type @float : f32 = #ada.float_info<digits 6>

  // CHECK-LABEL: ada.subp @objects
  ada.subp @objects(%v: !ada.qual<i32, @integer>) -> !ada.qual<i32, @integer> {
    // CHECK: %[[X:.*]] = ada.object {{.*}} <- %arg0
    %x = ada.object : !ada.ref<!ada.qual<i32, @integer>> <- %v : !ada.qual<i32, @integer>
    // CHECK: %[[Y:.*]] = ada.object {{.*}} <- %arg0
    %y = ada.object : !ada.ref<!ada.qual<i32, @integer>> <- %v : !ada.qual<i32, @integer>
    %one = ada.constant : !ada.qual<i32, @integer> = 1
    // CHECK: ada.write %{{.*}}, %[[X]]
    ada.write %one, %x : !ada.qual<i32, @integer> -> !ada.ref<!ada.qual<i32, @integer>>
    // CHECK: %[[A:.*]] = ada.read %[[X]]
    %a = ada.read %x : !ada.ref<!ada.qual<i32, @integer>> -> !ada.qual<i32, @integer>
    // CHECK: %[[B:.*]] = ada.read %[[Y]]
    %b = ada.read %y : !ada.ref<!ada.qual<i32, @integer>> -> !ada.qual<i32, @integer>
    // CHECK: ada.binop "+" %[[A]], %[[B]]
    %sum = ada.binop "+" %a, %b : (!ada.qual<i32, @integer>, !ada.qual<i32, @integer>) -> !ada.qual<i32, @integer>
    ada.return %sum : !ada.qual<i32, @integer>
  }

  // CHECK-LABEL: ada.subp @subtype
  ada.subp @subtype(%r: !ada.ref<!ada.qual<i32, @small>> {ada.mode = #ada<mode in_out>}, %v: !ada.qual<i32, @small>) -> !ada.qual<i32, @integer> {
    // CHECK: ada.write %arg1, %arg0 : !ada.qual<i32, @small> -> !ada.ref<!ada.qual<i32, @small>>
    ada.write %v, %r : !ada.qual<i32, @small> -> !ada.ref<!ada.qual<i32, @small>>
    // CHECK: ada.read %arg0 : !ada.ref<!ada.qual<i32, @small>> -> !ada.qual<i32, @integer>
    %base = ada.read %r : !ada.ref<!ada.qual<i32, @small>> -> !ada.qual<i32, @integer>
    ada.return %base : !ada.qual<i32, @integer>
  }

  // CHECK-LABEL: ada.subp @float_object
  ada.subp @float_object(%v: !ada.qual<f32, @float>) -> !ada.qual<f32, @float> {
    // CHECK: ada.object : !ada.ref<!ada.qual<f32, @float>>
    %r = ada.object : !ada.ref<!ada.qual<f32, @float>>
    ada.write %v, %r : !ada.qual<f32, @float> -> !ada.ref<!ada.qual<f32, @float>>
    %value = ada.read %r : !ada.ref<!ada.qual<f32, @float>> -> !ada.qual<f32, @float>
    ada.return %value : !ada.qual<f32, @float>
  }
}
