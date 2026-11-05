// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// RUN: %lalvm --emit=mlir -O1 %s | %FileCheck %s

module {
  ada.type @integer : i32 = #ada.int_info<range -2147483648 to 2147483647>

  // Only the last write can be observed by the caller.
  // CHECK-LABEL: ada.subp @overwritten(
  // CHECK-SAME: %[[REF:[^: ]+]]: !ada.ref<!ada.qual<i32, @integer>>
  // CHECK-SAME: %[[FIRST:[^: ]+]]: !ada.qual<i32, @integer>
  // CHECK-SAME: %[[LAST:[^: ]+]]: !ada.qual<i32, @integer>
  // CHECK-NOT: ada.write
  // CHECK: ada.write %[[LAST]], %[[REF]]
  // CHECK-NEXT: ada.return
  ada.subp @overwritten(%x: !ada.ref<!ada.qual<i32, @integer>> {ada.mode = #ada<mode in_out>}, %first: !ada.qual<i32, @integer>, %last: !ada.qual<i32, @integer>) {
    ada.write %first, %x : !ada.qual<i32, @integer> -> !ada.ref<!ada.qual<i32, @integer>>
    ada.write %last, %x : !ada.qual<i32, @integer> -> !ada.ref<!ada.qual<i32, @integer>>
    ada.return
  }

  // Writing back the value just read leaves the object unchanged.
  // CHECK-LABEL: ada.subp @write_back(
  // CHECK-NOT: ada.write
  // CHECK: ada.return
  ada.subp @write_back(%x: !ada.ref<!ada.qual<i32, @integer>> {ada.mode = #ada<mode in_out>}) {
    %value = ada.read %x : !ada.ref<!ada.qual<i32, @integer>> -> !ada.qual<i32, @integer>
    ada.write %value, %x : !ada.qual<i32, @integer> -> !ada.ref<!ada.qual<i32, @integer>>
    ada.return
  }

  // An intervening write through another reference may change the same object.
  // CHECK-LABEL: ada.subp @may_alias(
  // CHECK: %[[OLD:.*]] = ada.read
  // CHECK-NEXT: ada.write
  // CHECK-NEXT: ada.write %[[OLD]]
  // CHECK-NEXT: ada.return
  ada.subp @may_alias(%x: !ada.ref<!ada.qual<i32, @integer>> {ada.mode = #ada<mode in_out>}, %y: !ada.ref<!ada.qual<i32, @integer>> {ada.mode = #ada<mode in_out>}, %v: !ada.qual<i32, @integer>) {
    %old = ada.read %x : !ada.ref<!ada.qual<i32, @integer>> -> !ada.qual<i32, @integer>
    ada.write %v, %y : !ada.qual<i32, @integer> -> !ada.ref<!ada.qual<i32, @integer>>
    ada.write %old, %x : !ada.qual<i32, @integer> -> !ada.ref<!ada.qual<i32, @integer>>
    ada.return
  }

  // A read before the next write observes the earlier stored value.
  // CHECK-LABEL: ada.subp @observed(
  // CHECK: ada.write
  // CHECK-NEXT: %[[VALUE:.*]] = ada.read
  // CHECK-NEXT: ada.write
  // CHECK-NEXT: ada.return %[[VALUE]]
  ada.subp @observed(%x: !ada.ref<!ada.qual<i32, @integer>> {ada.mode = #ada<mode in_out>}, %y: !ada.ref<!ada.qual<i32, @integer>> {ada.mode = #ada<mode in_out>}, %v: !ada.qual<i32, @integer>, %w: !ada.qual<i32, @integer>) -> !ada.qual<i32, @integer> {
    ada.write %v, %x : !ada.qual<i32, @integer> -> !ada.ref<!ada.qual<i32, @integer>>
    %value = ada.read %y : !ada.ref<!ada.qual<i32, @integer>> -> !ada.qual<i32, @integer>
    ada.write %w, %x : !ada.qual<i32, @integer> -> !ada.ref<!ada.qual<i32, @integer>>
    ada.return %value : !ada.qual<i32, @integer>
  }

  // Overflow can raise before the second write, leaving the first observable.
  // CHECK-LABEL: ada.subp @checked_between(
  // CHECK: ada.write
  // CHECK-NEXT: %[[CHECKED:.*]] = ada.binop "+" {{.*}} checks<overflow>
  // CHECK-NEXT: ada.write
  // CHECK-NEXT: ada.return %[[CHECKED]]
  ada.subp @checked_between(%x: !ada.ref<!ada.qual<i32, @integer>> {ada.mode = #ada<mode in_out>}, %v: !ada.qual<i32, @integer>, %w: !ada.qual<i32, @integer>) -> !ada.qual<i32, @integer> {
    ada.write %v, %x : !ada.qual<i32, @integer> -> !ada.ref<!ada.qual<i32, @integer>>
    %checked = ada.binop "+" %v, %w checks<overflow> : (!ada.qual<i32, @integer>, !ada.qual<i32, @integer>) -> !ada.qual<i32, @integer>
    ada.write %w, %x : !ada.qual<i32, @integer> -> !ada.ref<!ada.qual<i32, @integer>>
    ada.return %checked : !ada.qual<i32, @integer>
  }
}
