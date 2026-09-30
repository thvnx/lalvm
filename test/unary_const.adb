-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm -O1 --emit=llvm %s | %FileCheck %s --implicit-check-not=with.overflow

-- After `mem2reg`, a checked unary `-` or `abs` on a known value cannot
-- overflow: it needs no overflow check and folds to a constant.

-- CHECK-LABEL: define i32 @_ada_unary_const(
-- CHECK:         ret i32 5

function Unary_Const return Integer is
   X : Integer := 5;
   Y : Integer := -X;
begin
   return abs Y;
end Unary_Const;
