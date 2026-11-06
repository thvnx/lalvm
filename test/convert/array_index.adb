-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=SOURCE --implicit-check-not=ada.index_check
-- RUN: %lalvm --emit=llvm --mlir-print-ir-after=ada-lower-indexing %s -o /dev/null 2>&1 | %FileCheck %s

-- Array index, checked into the index subtype.

-- SOURCE: ada.index %{{.*}}[%i]

-- CHECK: %[[CHK:.*]] = ada.index_check %{{.*}}, %{{.*}} : !ada.qual<i32, @standard.integer> to !ada.qual<i32, @array_index.__anonymous_vec_index_1>
-- CHECK: ada.index %{{.*}}[%[[CHK]]] offset %{{.*}} : (memref<!ada.qual<!ada.array<i32[i32 x 10]>, @array_index.vec>>, !ada.qual<i32, @array_index.__anonymous_vec_index_1>, i64) -> !ada.ref<!ada.qual<i32, @standard.integer>>

function Array_Index (I : Integer) return Integer is
   type Vec is array (1 .. 10) of Integer;
   V : Vec;
begin
   return V (I);
end Array_Index;
