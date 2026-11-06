// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// RUN: %lalvm --emit=llvm --mlir-print-ir-after=ada-lower-references %s -o /dev/null 2>&1 | %FileCheck %s --check-prefix=LOWER --implicit-check-not=ada.object --implicit-check-not=ada.read --implicit-check-not=ada.write --implicit-check-not=!ada.ref --implicit-check-not=unrealized_conversion_cast
// RUN: %lalvm --emit=llvm -O1 --mlir-print-ir-after=canonicalize %s -o /dev/null 2>&1 | %FileCheck %s --check-prefix=PROMOTE --implicit-check-not=ada.alloca --implicit-check-not=memref.alloca --implicit-check-not=memref.load --implicit-check-not=memref.store

module {
  ada.type @integer : i32 = #ada.int_info<range -2147483648 to 2147483647>
  ada.type @small base @integer : i32 = #ada.int_info<range 1 to 10>
  ada.type @float : f32 = #ada.float_info<digits 6>
  ada.type @nonzero : i32 = #ada.int_info<range 5 to 10>
  ada.type @color : i8 = #ada.enum_info<"red" = 7, "green" = 9>

  // LOWER-LABEL: func.func @_ada_subtype
  // LOWER: %[[SLOT:.*]] = memref.alloca() : memref<!ada.qual<i32, @integer>>
  // LOWER: %[[INITIAL:.*]] = ada.requalify %arg0 : !ada.qual<i32, @small> to !ada.qual<i32, @integer>
  // LOWER: memref.store %[[INITIAL]], %[[SLOT]][]
  // LOWER: %[[LOADED:.*]] = memref.load %[[SLOT]][]
  // LOWER: %[[NOMINAL:.*]] = ada.requalify %[[LOADED]] : !ada.qual<i32, @integer> to !ada.qual<i32, @small>
  // LOWER: return %[[NOMINAL]]
  // PROMOTE-LABEL: func.func @_ada_subtype
  // PROMOTE-NEXT: return %arg0
  ada.subp @subtype(%v: !ada.qual<i32, @small>) -> !ada.qual<i32, @small> {
    %r = ada.object : !ada.ref<!ada.qual<i32, @small>> <- %v : !ada.qual<i32, @small>
    %value = ada.read %r : !ada.ref<!ada.qual<i32, @small>> -> !ada.qual<i32, @small>
    ada.return %value : !ada.qual<i32, @small>
  }

  // LOWER-LABEL: func.func @_ada_float_object
  // LOWER: %[[FLOAT:.*]] = memref.alloca() : memref<!ada.qual<f32, @float>>
  // LOWER-NEXT: memref.store %arg0, %[[FLOAT]][]
  // LOWER-NEXT: %[[VALUE:.*]] = memref.load %[[FLOAT]][]
  // LOWER-NEXT: return %[[VALUE]]
  // PROMOTE-LABEL: func.func @_ada_float_object
  // PROMOTE-NEXT: return %arg0
  ada.subp @float_object(%v: !ada.qual<f32, @float>) -> !ada.qual<f32, @float> {
    %r = ada.object : !ada.ref<!ada.qual<f32, @float>>
    ada.write %v, %r : !ada.qual<f32, @float> -> !ada.ref<!ada.qual<f32, @float>>
    %value = ada.read %r : !ada.ref<!ada.qual<f32, @float>> -> !ada.qual<f32, @float>
    ada.return %value : !ada.qual<f32, @float>
  }

  // Promotion supplies a base value, without asserting membership in @small.
  // PROMOTE-LABEL: func.func @_ada_uninitialized
  // PROMOTE: %[[ZERO:.*]] = ada.constant : !ada.qual<i32, @integer> = 0
  // PROMOTE-NEXT: return %[[ZERO]]
  ada.subp @uninitialized() -> !ada.qual<i32, @integer> {
    %r = ada.object : !ada.ref<!ada.qual<i32, @small>>
    %value = ada.read %r : !ada.ref<!ada.qual<i32, @small>> -> !ada.qual<i32, @integer>
    ada.return %value : !ada.qual<i32, @integer>
  }

  // PROMOTE-LABEL: func.func @_ada_nonzero_default
  // PROMOTE: %[[FIRST:.*]] = ada.constant : !ada.qual<i32, @nonzero> = 5
  // PROMOTE-NEXT: return %[[FIRST]]
  ada.subp @nonzero_default() -> !ada.qual<i32, @nonzero> {
    %r = ada.object : !ada.ref<!ada.qual<i32, @nonzero>>
    %value = ada.read %r : !ada.ref<!ada.qual<i32, @nonzero>> -> !ada.qual<i32, @nonzero>
    ada.return %value : !ada.qual<i32, @nonzero>
  }

  // PROMOTE-LABEL: func.func @_ada_enum_default
  // PROMOTE: %[[RED:.*]] = ada.constant : !ada.qual<i8, @color> = 7
  // PROMOTE-NEXT: return %[[RED]]
  ada.subp @enum_default() -> !ada.qual<i8, @color> {
    %r = ada.object : !ada.ref<!ada.qual<i8, @color>>
    %value = ada.read %r : !ada.ref<!ada.qual<i8, @color>> -> !ada.qual<i8, @color>
    ada.return %value : !ada.qual<i8, @color>
  }
}
