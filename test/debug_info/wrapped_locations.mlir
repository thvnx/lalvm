// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// RUN: %lalvm -g --emit=llvm %s | %FileCheck %s

// Names and source positions remain available beneath location wrappers.
// The first fused child deliberately has no source position.

// CHECK-DAG: !DIFile(filename: "wrapped.adb", directory: "/src")
// CHECK-DAG: !DILocalVariable(name: "x", arg: 1, scope: {{.*}}, file: {{.*}}, line: 7,
// CHECK-DAG: !DILocalVariable(name: "v", scope: {{.*}}, file: {{.*}}, line: 9,
// CHECK-DAG: !DISubrangeType(name: "small", {{.*}}line: 4,

module @wrapped_locations {
  ada.type @integer : i32 = #ada.int_info<range -2147483648 to 2147483647>
  ada.type @small base @integer : i32 = #ada.int_info<range 1 to 10> loc(fused[unknown, "type"("wrapped.adb":4:3)])
  ada.subp @wrapped_locations(
      %x: !ada.qual<i32, @integer> loc(fused[unknown, "x"("wrapped.adb":7:3)])) -> !ada.qual<i32, @integer> {
    %v = ada.binop "+" %x, %x : (!ada.qual<i32, @integer>, !ada.qual<i32, @integer>) -> !ada.qual<i32, @integer> loc(fused[unknown, "v"(fused[unknown, "wrapped.adb":9:3])])
    ada.return %v : !ada.qual<i32, @integer>
  } loc("wrapped.adb":6:1)
  ada.subp @small_result(%x: !ada.qual<i32, @small>) -> !ada.qual<i32, @small> {
    ada.return %x : !ada.qual<i32, @small>
  } loc("wrapped.adb":12:1)
} loc("module"("/src/wrapped.adb":1:1))
