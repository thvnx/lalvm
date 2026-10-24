-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- Reading an array element with a dynamic index: the index is loaded from a
-- variable, so the zero-based offset is computed at run time.

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- CHECK: %[[I:.*]] = memref.load %{{.*}}[] : memref<!ada.qual<i32, @standard.integer>>
-- CHECK: %[[CHK:.*]] = ada.index_check %[[I]], %{{.*}} : !ada.qual<i32, @standard.integer> to !ada.qual<i32, @array_read_dyn.__anonymous_vec_index_1>
-- CHECK: %[[OFF:.*]] = ada.binop "-" %[[CHK]], %{{.*}} : (!ada.qual<i32, @array_read_dyn.__anonymous_vec_index_1>, !ada.qual<i32, @standard.integer>) -> !ada.qual<i32, @standard.integer>
-- CHECK: ada.index %{{.*}}[%[[OFF]]] {{.*}}-> memref<!ada.qual<i32, @standard.integer>, strided<[], offset: ?>>

procedure Array_Read_Dyn is
   type Vec is array (1 .. 10) of Integer;
   V : Vec;
   I : Integer;
   X : Integer;
begin
   X := V (I);
end Array_Read_Dyn;
