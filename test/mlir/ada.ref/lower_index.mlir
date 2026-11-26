// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// RUN: %lalvm --emit=llvm --mlir-print-ir-after=ada-lower-references %s -o /dev/null 2>&1 | %FileCheck %s --check-prefix=LOWER --implicit-check-not=!ada.ref --implicit-check-not=unrealized_conversion_cast
// RUN: %lalvm --emit=llvm --mlir-print-ir-after=ada-lower-indexing %s -o /dev/null 2>&1 | %FileCheck %s --check-prefix=INDEXING --implicit-check-not=memref.reinterpret_cast
// RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefixes=LLVM,NOOPT --implicit-check-not=unrealized_conversion_cast
// RUN: %lalvm --emit=llvm -O1 %s | %FileCheck %s --check-prefix=OPT

module {
  ada.type @integer : i32 = #ada.int_info<range -2147483648 to 2147483647>
  ada.type @small subtype @integer : i32 = #ada.int_info<range 1 to 10>
  ada.type @array : !ada.array<i32[i32 x 10]> = #ada.array_info<component @small, dim @integer range 1 to 10>

  // LOWER-LABEL: func.func @_ada_element
  // LOWER: %[[ARRAY:.*]] = memref.alloca()
  // LOWER: %[[ELEMENT:.*]] = ada.index %[[ARRAY]][%{{[^ ]+}}] offset %{{[^ ]+}}
  // LOWER-SAME: -> memref<!ada.qual<i32, @integer>>
  // INDEXING-LABEL: func.func @_ada_element(
  // INDEXING: %[[CHECKED:.*]] = ada.index_check %arg0,
  // INDEXING: %[[RAW:.*]] = ada.unwrap %[[CHECKED]]
  // INDEXING: arith.extsi %[[RAW]] : i32 to i64
  // INDEXING: %[[OFFSET:.*]] = arith.subi
  // INDEXING: ada.index %{{.*}}[%[[CHECKED]]] offset %[[OFFSET]]
  // INDEXING-SAME: -> !ada.ref<!ada.qual<i32, @small>>
  // LOWER: %[[BASE:.*]] = ada.requalify %arg1 : !ada.qual<i32, @small> to !ada.qual<i32, @integer>
  // LOWER: memref.store %[[BASE]], %[[ELEMENT]][]
  // LOWER: %[[VALUE:.*]] = memref.load %[[ELEMENT]][]
  // LOWER: return %[[VALUE]]
  // LLVM-LABEL: define i32 @_ada_element(
  // LLVM: %[[INDEX:.*]] = sext i32 %0 to i64
  // LLVM: %[[OFFSET:.*]] = sub i64 %[[INDEX]], 1
  // LLVM: getelementptr [10 x i32], ptr %{{.*}}, i32 0, i64 %[[OFFSET]]
  // LLVM: store i32 %1, ptr %[[PTR:.*]], align 4
  // NOOPT: %[[VALUE:.*]] = load i32, ptr %[[PTR]], align 4
  // NOOPT: ret i32 %[[VALUE]]
  ada.subp @element(%index: !ada.qual<i32, @integer>, %v: !ada.qual<i32, @small>) -> !ada.qual<i32, @integer> {
    %array = ada.object : !ada.ref<!ada.qual<!ada.array<i32[i32 x 10]>, @array>>
    %element = ada.index %array[%index] : (!ada.ref<!ada.qual<!ada.array<i32[i32 x 10]>, @array>>, !ada.qual<i32, @integer>) -> !ada.ref<!ada.qual<i32, @small>>
    ada.write %v, %element : !ada.qual<i32, @small> -> !ada.ref<!ada.qual<i32, @small>>
    %value = ada.read %element : !ada.ref<!ada.qual<i32, @small>> -> !ada.qual<i32, @integer>
    ada.return %value : !ada.qual<i32, @integer>
  }

  // An unused selection still checks its index.
  // INDEXING-LABEL: func.func @_ada_unused(
  // INDEXING: ada.index_check
  // LLVM-LABEL: define void @_ada_unused(
  // LLVM: call void @__lalvm_rcheck_CE_Index_Check
  ada.subp @unused(%index: !ada.qual<i32, @integer>) {
    %array = ada.object : !ada.ref<!ada.qual<!ada.array<i32[i32 x 10]>, @array>>
    %element = ada.index %array[%index] : (!ada.ref<!ada.qual<!ada.array<i32[i32 x 10]>, @array>>, !ada.qual<i32, @integer>) -> !ada.ref<!ada.qual<i32, @small>>
    ada.return
  }

  ada.type @byte : i8 = #ada.int_info<mod 256>
  ada.type @upper_byte subtype @byte : i8 = #ada.int_info<range 128 to 255>
  ada.type @bytes : !ada.array<i32[i8 x 128]> = #ada.array_info<component @integer, dim @upper_byte range 128 to 255>

  // Recognize an existing check even when its bounds set the machine sign bit.
  // INDEXING-LABEL: func.func @_ada_checked_unsigned(
  // INDEXING: %[[CHECKED_UNSIGNED:.*]] = ada.index_check
  // INDEXING-NEXT: %[[BYTES:.*]] = ada.object
  // INDEXING-NEXT: %[[RAW_UNSIGNED:.*]] = ada.unwrap %[[CHECKED_UNSIGNED]]
  // INDEXING-NEXT: arith.extui %[[RAW_UNSIGNED]] : i8 to i64
  // INDEXING: ada.index %[[BYTES]][%[[CHECKED_UNSIGNED]]] offset
  ada.subp @checked_unsigned(%index: !ada.qual<i8, @byte>) {
    %lo = ada.constant : !ada.qual<i8, @byte> = 128
    %hi = ada.constant : !ada.qual<i8, @byte> = 255
    %range = ada.range %lo, %hi : !ada.qual<i8, @byte> -> !ada.range<i8, @byte>
    %checked = ada.index_check %index, %range : !ada.qual<i8, @byte> to !ada.qual<i8, @byte>
    %array = ada.object : !ada.ref<!ada.qual<!ada.array<i32[i8 x 128]>, @bytes>>
    %element = ada.index %array[%checked] : (!ada.ref<!ada.qual<!ada.array<i32[i8 x 128]>, @bytes>>, !ada.qual<i8, @byte>) -> !ada.ref<!ada.qual<i32, @integer>>
    ada.return
  }
}

// OPT-LABEL: define {{.*}}void @_ada_unused(
// OPT: call void @__lalvm_rcheck_CE_Index_Check
