// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// RUN: %lalvm --emit=mlir -O1 %s | %FileCheck %s

module {
  ada.type @integer : i32 = #ada.int_info<range -2147483648 to 2147483647>
  ada.type @small base @integer : i32 = #ada.int_info<range 1 to 10>

  // The write remains observable, but the read can use its value directly.
  // CHECK-LABEL: ada.subp @nominal(
  // CHECK-SAME: %[[REF:[^: ]+]]: !ada.ref<!ada.qual<i32, @small>>
  // CHECK-SAME: %[[INPUT:[^: ]+]]: !ada.qual<i32, @small>
  // CHECK: ada.write %[[INPUT]], %[[REF]]
  // CHECK-NEXT: ada.return %[[INPUT]] : !ada.qual<i32, @small>
  ada.subp @nominal(%x: !ada.ref<!ada.qual<i32, @small>> {ada.mode = #ada<mode in_out>}, %v: !ada.qual<i32, @small>) -> !ada.qual<i32, @small> {
    ada.write %v, %x : !ada.qual<i32, @small> -> !ada.ref<!ada.qual<i32, @small>>
    %value = ada.read %x : !ada.ref<!ada.qual<i32, @small>> -> !ada.qual<i32, @small>
    ada.return %value : !ada.qual<i32, @small>
  }

  // Different reference arguments may designate the same object.
  // CHECK-LABEL: ada.subp @may_alias(
  // CHECK: ada.write
  // CHECK-NEXT: ada.write
  // CHECK-NEXT: %[[VALUE:.*]] = ada.read
  // CHECK-NEXT: ada.return %[[VALUE]]
  ada.subp @may_alias(%x: !ada.ref<!ada.qual<i32, @small>> {ada.mode = #ada<mode in_out>}, %y: !ada.ref<!ada.qual<i32, @small>> {ada.mode = #ada<mode in_out>}, %v: !ada.qual<i32, @small>, %w: !ada.qual<i32, @small>) -> !ada.qual<i32, @small> {
    ada.write %v, %x : !ada.qual<i32, @small> -> !ada.ref<!ada.qual<i32, @small>>
    ada.write %w, %y : !ada.qual<i32, @small> -> !ada.ref<!ada.qual<i32, @small>>
    %value = ada.read %x : !ada.ref<!ada.qual<i32, @small>> -> !ada.qual<i32, @small>
    ada.return %value : !ada.qual<i32, @small>
  }
}
