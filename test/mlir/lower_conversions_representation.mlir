// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// LowerAdaConversions on conversions to another base type.

// RUN: %lalvm --emit=llvm --mlir-print-ir-after=ada-lower-conversions %s -o /dev/null 2>&1 | %FileCheck %s --implicit-check-not=ada.convert
// RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

module {
  ada.type @tiny : i8 = #ada.int_info<range -100 to 100>
  ada.type @short_integer : i16 = #ada.int_info<range -32768 to 32767>
  ada.type @negative base @short_integer : i16 = #ada.int_info<range -100 to -1>
  ada.type @word : i16 = #ada.int_info<mod 65536>
  ada.type @integer : i32 = #ada.int_info<range -2147483648 to 2147483647>
  ada.type @positive base @integer : i32 = #ada.int_info<range 1 to 2147483647>
  ada.type @long_integer : i64 = #ada.int_info<range -9223372036854775808 to 9223372036854775807>
  ada.type @small base @long_integer : i64 = #ada.int_info<range 0 to 100>
  ada.type @float : f32 = #ada.float_info<digits 6>
  ada.type @long_float : f64 = #ada.float_info<digits 15>
  ada.type @my_float : f32 = #ada.float_info<digits 6>
  ada.type @color : i8 = #ada.enum_info<"red" = 0, "green" = 1, "blue" = 2>
  ada.type @hue : i8 = #ada.enum_info<"red" = 0, "green" = 1, "blue" = 2>

  // CHECK-LABEL: IR Dump After LowerAdaConversions

  // Widening, every value fits: no check.
  // CHECK-LABEL: ada.subp @widen_exact
  // CHECK-NOT:     ada.range_check
  // CHECK:         %[[U:.*]] = ada.unwrap %arg0 : !ada.qual<i16, @short_integer> to i16
  // CHECK-NEXT:    %[[E:.*]] = arith.extsi %[[U]] : i16 to i32
  // CHECK-NEXT:    %[[W:.*]] = ada.wrap %[[E]] : i32 to !ada.qual<i32, @integer>
  // CHECK-NEXT:    ada.return %[[W]]
  // LLVM-LABEL: define i32 @_ada_widen_exact(
  // LLVM:         sext i16 %0 to i32
  ada.subp @widen_exact(%v: !ada.qual<i16, @short_integer>) -> !ada.qual<i32, @integer> {
    %r = ada.convert implicit %v : !ada.qual<i16, @short_integer> to !ada.qual<i32, @integer>
    ada.return %r : !ada.qual<i32, @integer>
  }

  // Widening a modular value zero-extends it.
  // CHECK-LABEL: ada.subp @widen_modular
  // CHECK-NOT:     ada.range_check
  // CHECK:         %[[U:.*]] = ada.unwrap %arg0 : !ada.qual<i16, @word> to i16
  // CHECK-NEXT:    %[[E:.*]] = arith.extui %[[U]] : i16 to i32
  // CHECK-NEXT:    %[[W:.*]] = ada.wrap %[[E]] : i32 to !ada.qual<i32, @integer>
  // LLVM-LABEL: define i32 @_ada_widen_modular(
  // LLVM:         zext i16 %0 to i32
  ada.subp @widen_modular(%v: !ada.qual<i16, @word>) -> !ada.qual<i32, @integer> {
    %r = ada.convert explicit %v : !ada.qual<i16, @word> to !ada.qual<i32, @integer>
    ada.return %r : !ada.qual<i32, @integer>
  }

  // Widening into a constrained subtype: the check comes after.
  // CHECK-LABEL: ada.subp @widen_checked
  // CHECK:         %[[E:.*]] = arith.extsi %{{.*}} : i16 to i32
  // CHECK-NEXT:    %[[W:.*]] = ada.wrap %[[E]] : i32 to !ada.qual<i32, @integer>
  // CHECK:         %[[C:.*]] = ada.range_check %[[W]], %{{.*}} : !ada.qual<i32, @integer> to !ada.qual<i32, @positive>
  // CHECK-NEXT:    ada.return %[[C]]
  ada.subp @widen_checked(%v: !ada.qual<i16, @short_integer>) -> !ada.qual<i32, @positive> {
    %r = ada.convert explicit %v : !ada.qual<i16, @short_integer> to !ada.qual<i32, @positive>
    ada.return %r : !ada.qual<i32, @positive>
  }

  // Widening a signed value into a modular type: the check comes first, with
  // the target range restricted to the source representation.
  // CHECK-LABEL: ada.subp @widen_into_modular
  // CHECK:         %[[LO:.*]] = ada.constant : !ada.qual<i8, @tiny> = 0
  // CHECK:         %[[HI:.*]] = ada.constant : !ada.qual<i8, @tiny> = 127
  // CHECK:         %[[R:.*]] = ada.range %[[LO]], %[[HI]]
  // CHECK:         %[[C:.*]] = ada.range_check %arg0, %[[R]] : !ada.qual<i8, @tiny> to !ada.qual<i8, @tiny>
  // CHECK:         %[[U:.*]] = ada.unwrap %[[C]] : !ada.qual<i8, @tiny> to i8
  // CHECK-NEXT:    %[[E:.*]] = arith.extsi %[[U]] : i8 to i16
  // CHECK-NEXT:    %[[W:.*]] = ada.wrap %[[E]] : i16 to !ada.qual<i16, @word>
  // CHECK-NEXT:    ada.return %[[W]]
  ada.subp @widen_into_modular(%v: !ada.qual<i8, @tiny>) -> !ada.qual<i16, @word> {
    %r = ada.convert explicit %v : !ada.qual<i8, @tiny> to !ada.qual<i16, @word>
    ada.return %r : !ada.qual<i16, @word>
  }

  // Narrowing: the check comes first, on the original value.
  // CHECK-LABEL: ada.subp @narrow
  // CHECK:         %[[LO:.*]] = ada.constant : !ada.qual<i32, @integer> = -32768
  // CHECK:         %[[HI:.*]] = ada.constant : !ada.qual<i32, @integer> = 32767
  // CHECK:         %[[R:.*]] = ada.range %[[LO]], %[[HI]]
  // CHECK:         %[[C:.*]] = ada.range_check %arg0, %[[R]] : !ada.qual<i32, @integer> to !ada.qual<i32, @integer>
  // CHECK:         %[[U:.*]] = ada.unwrap %[[C]] : !ada.qual<i32, @integer> to i32
  // CHECK-NEXT:    %[[T:.*]] = arith.trunci %[[U]] : i32 to i16
  // CHECK-NEXT:    %[[W:.*]] = ada.wrap %[[T]] : i16 to !ada.qual<i16, @short_integer>
  // CHECK-NEXT:    ada.return %[[W]]
  // LLVM-LABEL: define i16 @_ada_narrow(
  // LLVM:         call void @__gnat_rcheck_CE_Range_Check(
  // LLVM:         trunc i32 %0 to i16
  ada.subp @narrow(%v: !ada.qual<i32, @integer>) -> !ada.qual<i16, @short_integer> {
    %r = ada.convert explicit %v : !ada.qual<i32, @integer> to !ada.qual<i16, @short_integer>
    ada.return %r : !ada.qual<i16, @short_integer>
  }

  // Narrowing into a constrained subtype: check, convert, then requalify.
  // CHECK-LABEL: ada.subp @narrow_into_subtype
  // CHECK:         %[[LO:.*]] = ada.constant : !ada.qual<i64, @long_integer> = 1
  // CHECK:         %[[HI:.*]] = ada.constant : !ada.qual<i64, @long_integer> = 2147483647
  // CHECK:         %[[C:.*]] = ada.range_check %arg0, %{{.*}} : !ada.qual<i64, @long_integer> to !ada.qual<i64, @long_integer>
  // CHECK:         %[[U:.*]] = ada.unwrap %[[C]]
  // CHECK-NEXT:    %[[T:.*]] = arith.trunci %[[U]] : i64 to i32
  // CHECK-NEXT:    %[[W:.*]] = ada.wrap %[[T]] : i32 to !ada.qual<i32, @integer>
  // CHECK-NEXT:    %[[Q:.*]] = ada.requalify %[[W]] : !ada.qual<i32, @integer> to !ada.qual<i32, @positive>
  // CHECK-NEXT:    ada.return %[[Q]]
  ada.subp @narrow_into_subtype(%v: !ada.qual<i64, @long_integer>) -> !ada.qual<i32, @positive> {
    %r = ada.convert explicit %v : !ada.qual<i64, @long_integer> to !ada.qual<i32, @positive>
    ada.return %r : !ada.qual<i32, @positive>
  }

  // Narrowing, every value fits: no check.
  // CHECK-LABEL: ada.subp @narrow_exact
  // CHECK-NOT:     ada.range_check
  // CHECK:         %[[U:.*]] = ada.unwrap %arg0 : !ada.qual<i64, @small> to i64
  // CHECK-NEXT:    %[[T:.*]] = arith.trunci %[[U]] : i64 to i32
  // CHECK-NEXT:    %[[W:.*]] = ada.wrap %[[T]] : i32 to !ada.qual<i32, @integer>
  // CHECK-NEXT:    ada.return %[[W]]
  ada.subp @narrow_exact(%v: !ada.qual<i64, @small>) -> !ada.qual<i32, @integer> {
    %r = ada.convert explicit %v : !ada.qual<i64, @small> to !ada.qual<i32, @integer>
    ada.return %r : !ada.qual<i32, @integer>
  }

  // A constant within the target's range is not checked.
  // CHECK-LABEL: ada.subp @narrow_constant
  // CHECK-NOT:     ada.range_check
  // CHECK:         arith.trunci %{{.*}} : i32 to i16
  ada.subp @narrow_constant() -> !ada.qual<i16, @short_integer> {
    %c = ada.constant : !ada.qual<i32, @integer> = 100
    %r = ada.convert explicit %c : !ada.qual<i32, @integer> to !ada.qual<i16, @short_integer>
    ada.return %r : !ada.qual<i16, @short_integer>
  }

  // Another base type of the same width: the check comes first, and the
  // machine value is kept.
  // CHECK-LABEL: ada.subp @other_base
  // CHECK:         %[[LO:.*]] = ada.constant : !ada.qual<i16, @word> = 0
  // CHECK:         %[[HI:.*]] = ada.constant : !ada.qual<i16, @word> = 32767
  // CHECK:         %[[C:.*]] = ada.range_check %arg0, %{{.*}} : !ada.qual<i16, @word> to !ada.qual<i16, @word>
  // CHECK:         %[[U:.*]] = ada.unwrap %[[C]] : !ada.qual<i16, @word> to i16
  // CHECK-NEXT:    %[[W:.*]] = ada.wrap %[[U]] : i16 to !ada.qual<i16, @short_integer>
  // CHECK-NEXT:    ada.return %[[W]]
  ada.subp @other_base(%v: !ada.qual<i16, @word>) -> !ada.qual<i16, @short_integer> {
    %r = ada.convert explicit %v : !ada.qual<i16, @word> to !ada.qual<i16, @short_integer>
    ada.return %r : !ada.qual<i16, @short_integer>
  }

  // Widening into a dynamic subtype: the check uses the descriptor the
  // conversion carries.
  // CHECK-LABEL: ada.subp @widen_dynamic
  // CHECK:         %[[R:.*]] = ada.range %{{.*}}, %arg0
  // CHECK-NOT:     ada.range_check
  // CHECK:         %[[E:.*]] = arith.extsi %{{.*}} : i16 to i32
  // CHECK-NEXT:    %[[W:.*]] = ada.wrap %[[E]] : i32 to !ada.qual<i32, @integer>
  // CHECK-NEXT:    %[[D:.*]] = ada.range_check %[[W]], %[[R]] : !ada.qual<i32, @integer> to !ada.qual<i32, @widen_dynamic.s>
  // CHECK-NEXT:    ada.return %[[D]]
  ada.subp @widen_dynamic(%n: !ada.qual<i32, @integer>, %v: !ada.qual<i16, @short_integer>) -> !ada.qual<i32, @widen_dynamic.s> {
    %lo = ada.constant : !ada.qual<i32, @integer> = 1
    %r = ada.range %lo, %n : !ada.qual<i32, @integer> -> !ada.range<i32, @widen_dynamic.s>
    ada.decls {
      ada.type @widen_dynamic.s base @integer : i32 = #ada.int_info<range 1 to ?>
    }
    %c = ada.convert explicit %v, %r : !ada.qual<i16, @short_integer> to !ada.qual<i32, @widen_dynamic.s>
    ada.return %c : !ada.qual<i32, @widen_dynamic.s>
  }

  // Narrowing into a dynamic subtype: the value is first checked against the
  // target's base, then against the descriptor.
  // CHECK-LABEL: ada.subp @narrow_dynamic
  // CHECK:         %[[R:.*]] = ada.range %{{.*}}, %arg0
  // CHECK:         %[[LO:.*]] = ada.constant : !ada.qual<i64, @long_integer> = -2147483648
  // CHECK:         %[[HI:.*]] = ada.constant : !ada.qual<i64, @long_integer> = 2147483647
  // CHECK:         %[[C:.*]] = ada.range_check %arg1, %{{.*}} : !ada.qual<i64, @long_integer> to !ada.qual<i64, @long_integer>
  // CHECK:         %[[T:.*]] = arith.trunci %{{.*}} : i64 to i32
  // CHECK-NEXT:    %[[W:.*]] = ada.wrap %[[T]] : i32 to !ada.qual<i32, @integer>
  // CHECK-NEXT:    %[[D:.*]] = ada.range_check %[[W]], %[[R]] : !ada.qual<i32, @integer> to !ada.qual<i32, @narrow_dynamic.s>
  // CHECK-NEXT:    ada.return %[[D]]
  ada.subp @narrow_dynamic(%n: !ada.qual<i32, @integer>, %v: !ada.qual<i64, @long_integer>) -> !ada.qual<i32, @narrow_dynamic.s> {
    %lo = ada.constant : !ada.qual<i32, @integer> = 1
    %r = ada.range %lo, %n : !ada.qual<i32, @integer> -> !ada.range<i32, @narrow_dynamic.s>
    ada.decls {
      ada.type @narrow_dynamic.s base @integer : i32 = #ada.int_info<range 1 to ?>
    }
    %c = ada.convert explicit %v, %r : !ada.qual<i64, @long_integer> to !ada.qual<i32, @narrow_dynamic.s>
    ada.return %c : !ada.qual<i32, @narrow_dynamic.s>
  }

  // Floats change precision; their constraints are not checked yet.
  // CHECK-LABEL: ada.subp @float_widen
  // CHECK-NOT:     ada.range_check
  // CHECK:         %[[U:.*]] = ada.unwrap %arg0 : !ada.qual<f32, @float> to f32
  // CHECK-NEXT:    %[[E:.*]] = arith.extf %[[U]] : f32 to f64
  // CHECK-NEXT:    %[[W:.*]] = ada.wrap %[[E]] : f64 to !ada.qual<f64, @long_float>
  // CHECK-NEXT:    ada.return %[[W]]
  ada.subp @float_widen(%v: !ada.qual<f32, @float>) -> !ada.qual<f64, @long_float> {
    %r = ada.convert implicit %v : !ada.qual<f32, @float> to !ada.qual<f64, @long_float>
    ada.return %r : !ada.qual<f64, @long_float>
  }
  // CHECK-LABEL: ada.subp @float_narrow
  // CHECK-NOT:     ada.range_check
  // CHECK:         arith.truncf %{{.*}} : f64 to f32
  ada.subp @float_narrow(%v: !ada.qual<f64, @long_float>) -> !ada.qual<f32, @float> {
    %r = ada.convert explicit %v : !ada.qual<f64, @long_float> to !ada.qual<f32, @float>
    ada.return %r : !ada.qual<f32, @float>
  }

  // Two narrowings from one source base get their own descriptor, reused by an
  // identical one.
  // CHECK-LABEL: ada.subp @narrow_twice
  // CHECK:         ada.constant : !ada.qual<i32, @integer> = -32768
  // CHECK:         ada.constant : !ada.qual<i32, @integer> = 32767
  // CHECK:         %[[R1:.*]] = ada.range %
  // CHECK:         ada.range_check %arg0, %[[R1]]
  // CHECK:         ada.constant : !ada.qual<i32, @integer> = -100
  // CHECK:         ada.constant : !ada.qual<i32, @integer> = 100
  // CHECK:         %[[R2:.*]] = ada.range %
  // CHECK:         ada.range_check %arg0, %[[R2]]
  // CHECK-NOT:     = ada.range %
  // CHECK:         ada.range_check %arg0, %[[R2]]
  ada.subp @narrow_twice(%v: !ada.qual<i32, @integer>) -> !ada.qual<i8, @tiny> {
    %s = ada.convert explicit %v : !ada.qual<i32, @integer> to !ada.qual<i16, @short_integer>
    %t = ada.convert explicit %v : !ada.qual<i32, @integer> to !ada.qual<i8, @tiny>
    %u = ada.convert explicit %v : !ada.qual<i32, @integer> to !ada.qual<i8, @tiny>
    ada.return %u : !ada.qual<i8, @tiny>
  }

  // Narrowing into a dynamic subtype of a modular type: the first check is
  // against the modular base's range.
  // CHECK-LABEL: ada.subp @narrow_modular_dynamic
  // CHECK:         %[[R:.*]] = ada.range %{{.*}}, %arg0
  // CHECK:         ada.constant : !ada.qual<i32, @integer> = 0
  // CHECK:         ada.constant : !ada.qual<i32, @integer> = 65535
  // CHECK:         ada.range_check %arg1, %{{.*}} : !ada.qual<i32, @integer> to !ada.qual<i32, @integer>
  // CHECK:         %[[T:.*]] = arith.trunci %{{.*}} : i32 to i16
  // CHECK-NEXT:    %[[W:.*]] = ada.wrap %[[T]] : i16 to !ada.qual<i16, @word>
  // CHECK-NEXT:    %[[D:.*]] = ada.range_check %[[W]], %[[R]] : !ada.qual<i16, @word> to !ada.qual<i16, @narrow_modular_dynamic.s>
  ada.subp @narrow_modular_dynamic(%n: !ada.qual<i16, @word>, %v: !ada.qual<i32, @integer>) -> !ada.qual<i16, @narrow_modular_dynamic.s> {
    %lo = ada.constant : !ada.qual<i16, @word> = 1
    %r = ada.range %lo, %n : !ada.qual<i16, @word> -> !ada.range<i16, @narrow_modular_dynamic.s>
    ada.decls {
      ada.type @narrow_modular_dynamic.s base @word : i16 = #ada.int_info<range 1 to ?>
    }
    %c = ada.convert explicit %v, %r : !ada.qual<i32, @integer> to !ada.qual<i16, @narrow_modular_dynamic.s>
    ada.return %c : !ada.qual<i16, @narrow_modular_dynamic.s>
  }

  // Another float or enumeration base type of the same width: the machine value
  // is kept, unchecked.
  // CHECK-LABEL: ada.subp @other_float
  // CHECK-NOT:     ada.range_check
  // CHECK:         %[[U:.*]] = ada.unwrap %arg0 : !ada.qual<f32, @float> to f32
  // CHECK-NEXT:    %[[W:.*]] = ada.wrap %[[U]] : f32 to !ada.qual<f32, @my_float>
  // CHECK-NEXT:    ada.return %[[W]]
  ada.subp @other_float(%v: !ada.qual<f32, @float>) -> !ada.qual<f32, @my_float> {
    %r = ada.convert explicit %v : !ada.qual<f32, @float> to !ada.qual<f32, @my_float>
    ada.return %r : !ada.qual<f32, @my_float>
  }
  // CHECK-LABEL: ada.subp @other_enum
  // CHECK-NOT:     ada.range_check
  // CHECK:         %[[U:.*]] = ada.unwrap %arg0 : !ada.qual<i8, @color> to i8
  // CHECK-NEXT:    %[[W:.*]] = ada.wrap %[[U]] : i8 to !ada.qual<i8, @hue>
  // CHECK-NEXT:    ada.return %[[W]]
  ada.subp @other_enum(%v: !ada.qual<i8, @color>) -> !ada.qual<i8, @hue> {
    %r = ada.convert explicit %v : !ada.qual<i8, @color> to !ada.qual<i8, @hue>
    ada.return %r : !ada.qual<i8, @hue>
  }

  // No value of the source lies in the target range: the restricted range is
  // empty, so the check always fails.
  // CHECK-LABEL: ada.subp @empty_range
  // CHECK:         ada.constant : !ada.qual<i16, @word> = -1
  // CHECK:         ada.constant : !ada.qual<i16, @word> = 0
  // CHECK:         ada.range_check %arg0, %{{.*}} : !ada.qual<i16, @word> to !ada.qual<i16, @word>
  ada.subp @empty_range(%v: !ada.qual<i16, @word>) -> !ada.qual<i16, @negative> {
    %r = ada.convert explicit %v : !ada.qual<i16, @word> to !ada.qual<i16, @negative>
    ada.return %r : !ada.qual<i16, @negative>
  }
}
