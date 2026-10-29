// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// RUN: %lalvm --emit=llvm --mlir-print-ir-after=ada-lower-references %s -o /dev/null 2>&1 | %FileCheck %s --check-prefix=LOWER --implicit-check-not=!ada.ref --implicit-check-not=unrealized_conversion_cast
// RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM
// RUN: %lalvm --emit=llvm -O1 %s | %FileCheck %s --check-prefix=LLVM

module {
  ada.type @integer : i32 = #ada.int_info<range -2147483648 to 2147483647>
  ada.type @small base @integer : i32 = #ada.int_info<range 1 to 10>
  ada.type @array : !ada.array<i32[i32 x 10]> = #ada.array_info<component @small, dim @integer range 1 to 10>

  // LOWER-LABEL: ada.subp @element
  // LOWER: %[[ARRAY:.*]] = ada.alloca
  // LOWER: %[[ELEMENT:.*]] = ada.index %[[ARRAY]][%arg0]
  // LOWER-SAME: -> memref<!ada.qual<i32, @integer>>
  // LOWER: %[[BASE:.*]] = ada.requalify %arg1 : !ada.qual<i32, @small> to !ada.qual<i32, @integer>
  // LOWER: memref.store %[[BASE]], %[[ELEMENT]][]
  // LOWER: %[[VALUE:.*]] = memref.load %[[ELEMENT]][]
  // LOWER: ada.return %[[VALUE]]
  // LLVM-LABEL: define i32 @_ada_element(
  // LLVM: getelementptr [10 x i32], ptr %{{.*}}, i32 0, i32 %0
  // LLVM: store i32 %1, ptr %[[PTR:.*]], align 4
  // LLVM: %[[VALUE:.*]] = load i32, ptr %[[PTR]], align 4
  // LLVM: ret i32 %[[VALUE]]
  ada.subp @element(%offset: !ada.qual<i32, @integer>, %v: !ada.qual<i32, @small>) -> !ada.qual<i32, @integer> {
    %array = ada.alloca : memref<!ada.qual<!ada.array<i32[i32 x 10]>, @array>>
    %element = ada.index %array[%offset] : (memref<!ada.qual<!ada.array<i32[i32 x 10]>, @array>>, !ada.qual<i32, @integer>) -> !ada.ref<!ada.qual<i32, @small>>
    ada.write %v, %element : !ada.qual<i32, @small> -> !ada.ref<!ada.qual<i32, @small>>
    %value = ada.read %element : !ada.ref<!ada.qual<i32, @small>> -> !ada.qual<i32, @integer>
    ada.return %value : !ada.qual<i32, @integer>
  }
}
