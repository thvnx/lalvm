-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm -O1 --emit=llvm %s | %FileCheck %s

-- At -O1, -X on the constant Integer'First keeps its overflow check.

-- CHECK-LABEL: define i32 @_ada_unary_minus_overflow_const(
-- CHECK:         @llvm.ssub.with.overflow.i32(
-- CHECK:         @__gnat_rcheck_CE_Overflow_Check(

function Unary_Minus_Overflow_Const return Integer is
   X : Integer := Integer'First;
begin
   return -X;
end Unary_Minus_Overflow_Const;
