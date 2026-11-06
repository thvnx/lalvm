-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=SOURCE --implicit-check-not=ada.index_check --implicit-check-not=" offset "
-- RUN: %lalvm --emit=llvm --mlir-print-ir-after=ada-lower-indexing %s -o /dev/null 2>&1 | %FileCheck %s --check-prefix=LOWER

-- A dynamic Ada index is checked against the array index subtype,
-- for a read and for a write. A static index in range needs no check.

-- SOURCE: ada.index

-- LOWER:       %[[I:.*]] = ada.read %{{.*}} : !ada.ref<!ada.qual<i32, @standard.integer>> -> !ada.qual<i32, @standard.integer>
-- LOWER:       ada.index_check %{{.*}}, %{{.*}}
-- LOWER-NOT:   ada.binop "-"
-- LOWER:       ada.index %

-- LOWER:       ada.index_check
-- LOWER-NOT:   ada.binop "-"
-- LOWER:       ada.index %
-- LOWER:       ada.write

-- LOWER-NOT:   ada.index_check
-- LOWER:       ada.index %
-- LOWER-NOT:   ada.index_check
-- LOWER:       return

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
