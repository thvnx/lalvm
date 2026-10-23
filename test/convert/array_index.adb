-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- Array index, checked into the index subtype and coerced back.

-- CHECK: ada.index_check %arg0, %{{.*}} : !ada.qual<i32, @standard.integer> to !ada.qual<i32, @array_index.__anonymous_vec_index_1>
-- CHECK: ada.coerce %{{.*}} : <i32, @array_index.__anonymous_vec_index_1> to <i32, @standard.integer>

function Array_Index (I : Integer) return Integer is
   type Vec is array (1 .. 10) of Integer;
   V : Vec;
begin
   return V (I);
end Array_Index;
