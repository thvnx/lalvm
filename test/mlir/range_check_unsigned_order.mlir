// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// A range check compares as its type orders (RM 13.4): unsigned for an
// enumeration without a negative code, so a Character above 127 is not
// negative, signed for one with a negative code.

// RUN: %lalvm --emit=llvm %s | %FileCheck %s

// CHECK-LABEL: define i8 @_ada_upper(
// CHECK:         icmp ult i8 %0, -64
// CHECK-NEXT:    icmp ugt i8 %0, -1

// CHECK-LABEL: define i2 @_ada_positive(
// CHECK:         icmp slt i2 %0, 0
// CHECK-NEXT:    icmp sgt i2 %0, 1
module {
  ada.type @character : i8 = #ada.enum_info<"a" = 97, "y_diaeresis" = 255>
  ada.type @latin_upper subtype @character : i8 = #ada.enum_info<range 192 to 255>
  ada.type @signed : i2 = #ada.enum_info<"neg" = -1, "zero" = 0, "pos" = 1>
  ada.type @nonnegative subtype @signed : i2 = #ada.enum_info<range 0 to 1>

  ada.subp @upper(%c: !ada.qual<i8, @character>) -> !ada.qual<i8, @latin_upper> {
    %lo = ada.constant : !ada.qual<i8, @character> = -64
    %hi = ada.constant : !ada.qual<i8, @character> = -1
    %r = ada.range %lo, %hi : !ada.qual<i8, @character> -> !ada.range<i8, @latin_upper>
    %v = ada.range_check %c, %r : !ada.qual<i8, @character> to !ada.qual<i8, @latin_upper>
    ada.return %v : !ada.qual<i8, @latin_upper>
  }

  ada.subp @positive(%s: !ada.qual<i2, @signed>) -> !ada.qual<i2, @nonnegative> {
    %lo = ada.constant : !ada.qual<i2, @signed> = 0
    %hi = ada.constant : !ada.qual<i2, @signed> = 1
    %r = ada.range %lo, %hi : !ada.qual<i2, @signed> -> !ada.range<i2, @nonnegative>
    %v = ada.range_check %s, %r : !ada.qual<i2, @signed> to !ada.qual<i2, @nonnegative>
    ada.return %v : !ada.qual<i2, @nonnegative>
  }
}
