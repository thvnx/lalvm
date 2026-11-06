-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- A dynamic Ada index is checked against the array index subtype,
-- for a read and for a write. A static index in range needs no check.

-- CHECK:       %[[I:.*]] = ada.read %{{.*}} : !ada.ref<!ada.qual<i32, @standard.integer>> -> !ada.qual<i32, @standard.integer>
-- CHECK:       ada.index_check %{{.*}}, %{{.*}}
-- CHECK-NOT:   ada.binop "-"
-- CHECK:       ada.index

-- CHECK:       ada.index_check
-- CHECK-NOT:   ada.binop "-"
-- CHECK:       ada.index
-- CHECK:       ada.write

-- CHECK-NOT:   ada.index_check
-- CHECK:       ada.index
-- CHECK-NOT:   ada.index_check
-- CHECK:       ada.return

procedure Array_Index_Check is
   type Vec is array (1 .. 10) of Integer;
   V : Vec;
   I : Integer;
   X : Integer;
begin
   X := V (I);
   V (I) := X;
   X := V (3);
end Array_Index_Check;
