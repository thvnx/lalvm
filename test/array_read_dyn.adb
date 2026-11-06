-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- Reading an array element with a dynamic index: the index is loaded from a
-- variable. Indexing lowering checks it before normalization.

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=SOURCE --implicit-check-not=ada.index_check
-- RUN: %lalvm --emit=llvm --mlir-print-ir-after=ada-lower-indexing %s -o /dev/null 2>&1 | %FileCheck %s

-- SOURCE: %[[I:.*]] = ada.read
-- SOURCE: ada.index %{{.*}}[%[[I]]]

-- CHECK: %[[I:.*]] = ada.read %{{.*}} : !ada.ref<!ada.qual<i32, @standard.integer>> -> !ada.qual<i32, @standard.integer>
-- CHECK: %[[CHK:.*]] = ada.index_check %[[I]], %{{.*}} : !ada.qual<i32, @standard.integer> to !ada.qual<i32, @array_read_dyn.__anonymous_vec_index_1>
-- CHECK: ada.index %{{.*}}[%[[CHK]]] {{.*}}-> !ada.ref<!ada.qual<i32, @standard.integer>>

procedure Array_Read_Dyn is
   type Vec is array (1 .. 10) of Integer;
   V : Vec;
   I : Integer;
   X : Integer;
begin
   X := V (I);
end Array_Read_Dyn;
