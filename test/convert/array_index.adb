-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- Array index, checked into the index subtype.

-- CHECK: %[[CHK:.*]] = ada.index_check %i, %{{.*}} : !ada.qual<i32, @standard.integer> to !ada.qual<i32, @array_index.__anonymous_vec_index_1>
-- CHECK-NEXT: {{.*}}ada.index %{{.*}}[%[[CHK]]] : (memref<!ada.qual<!ada.array<i32[i32 x 10]>, @array_index.vec>>, !ada.qual<i32, @array_index.__anonymous_vec_index_1>) -> !ada.ref<!ada.qual<i32, @standard.integer>>

function Array_Index (I : Integer) return Integer is
   type Vec is array (1 .. 10) of Integer;
   V : Vec;
begin
   return V (I);
end Array_Index;
