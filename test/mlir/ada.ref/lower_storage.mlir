// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// RUN: %lalvm --emit=llvm --mlir-print-ir-after=ada-lower-references %s -o /dev/null 2>&1 | %FileCheck %s --check-prefix=LOWER --implicit-check-not=ada.object --implicit-check-not=ada.read --implicit-check-not=ada.write --implicit-check-not=!ada.ref --implicit-check-not=unrealized_conversion_cast

module {
  ada.type @integer : i32 = #ada.int_info<range -2147483648 to 2147483647>
  ada.type @small base @integer : i32 = #ada.int_info<range 1 to 10>
  ada.type @float : f32 = #ada.float_info<digits 6>

  // LOWER-LABEL: ada.subp @subtype
  // LOWER: %[[SLOT:.*]] = ada.alloca : memref<!ada.qual<i32, @integer>>
  // LOWER: %[[INITIAL:.*]] = ada.requalify %arg0 : !ada.qual<i32, @small> to !ada.qual<i32, @integer>
  // LOWER: memref.store %[[INITIAL]], %[[SLOT]][]
  // LOWER: %[[LOADED:.*]] = memref.load %[[SLOT]][]
  // LOWER: %[[NOMINAL:.*]] = ada.requalify %[[LOADED]] : !ada.qual<i32, @integer> to !ada.qual<i32, @small>
  // LOWER: ada.return %[[NOMINAL]]
  ada.subp @subtype(%v: !ada.qual<i32, @small>) -> !ada.qual<i32, @small> {
    %r = ada.object : !ada.ref<!ada.qual<i32, @small>> <- %v : !ada.qual<i32, @small>
    %value = ada.read %r : !ada.ref<!ada.qual<i32, @small>> -> !ada.qual<i32, @small>
    ada.return %value : !ada.qual<i32, @small>
  }

  // LOWER-LABEL: ada.subp @float_object
  // LOWER: %[[FLOAT:.*]] = ada.alloca : memref<!ada.qual<f32, @float>>
  // LOWER-NEXT: memref.store %arg0, %[[FLOAT]][]
  // LOWER-NEXT: %[[VALUE:.*]] = memref.load %[[FLOAT]][]
  // LOWER-NEXT: ada.return %[[VALUE]]
  ada.subp @float_object(%v: !ada.qual<f32, @float>) -> !ada.qual<f32, @float> {
    %r = ada.object : !ada.ref<!ada.qual<f32, @float>>
    ada.write %v, %r : !ada.qual<f32, @float> -> !ada.ref<!ada.qual<f32, @float>>
    %value = ada.read %r : !ada.ref<!ada.qual<f32, @float>> -> !ada.qual<f32, @float>
    ada.return %value : !ada.qual<f32, @float>
  }
}
