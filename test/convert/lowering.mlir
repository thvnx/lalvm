// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// RUN: %lalvm --emit=llvm %s | %FileCheck %s

// Lowering of widening and narrowing, signed, modular and float.

module {
  ada.type @short_integer : i16 = #ada.int_info<range -32768 to 32767>
  ada.type @integer : i32 = #ada.int_info<range -2147483648 to 2147483647>
  ada.type @byte : i8 = #ada.int_info<mod 256>
  ada.type @float : f32 = #ada.float_info<digits 6>
  ada.type @long_float : f64 = #ada.float_info<digits 15>

  // CHECK: sext i16 %0 to i32
  ada.subp @sext(%x: !ada.qual<i16, @short_integer>) -> !ada.qual<i32, @integer> {
    %r = ada.coerce %x : <i16, @short_integer> to <i32, @integer>
    ada.return %r : !ada.qual<i32, @integer>
  }

  // CHECK: zext i8 %0 to i32
  ada.subp @zext(%x: !ada.qual<i8, @byte>) -> !ada.qual<i32, @integer> {
    %r = ada.coerce %x : <i8, @byte> to <i32, @integer>
    ada.return %r : !ada.qual<i32, @integer>
  }

  // CHECK: trunc i32 %0 to i8
  ada.subp @trunc(%x: !ada.qual<i32, @integer>) -> !ada.qual<i8, @byte> {
    %r = ada.coerce %x : <i32, @integer> to <i8, @byte>
    ada.return %r : !ada.qual<i8, @byte>
  }

  // CHECK: fpext float %0 to double
  ada.subp @fpext(%x: !ada.qual<f32, @float>) -> !ada.qual<f64, @long_float> {
    %r = ada.coerce %x : <f32, @float> to <f64, @long_float>
    ada.return %r : !ada.qual<f64, @long_float>
  }

  // CHECK: fptrunc double %0 to float
  ada.subp @fptrunc(%x: !ada.qual<f64, @long_float>) -> !ada.qual<f32, @float> {
    %r = ada.coerce %x : <f64, @long_float> to <f32, @float>
    ada.return %r : !ada.qual<f32, @float>
  }
}
