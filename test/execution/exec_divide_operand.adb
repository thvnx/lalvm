-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- REQUIRES: cc
-- RUN: %lalvm --emit=obj %s -o %t.o
-- RUN: %lalvm --bind %s -o %t.b.o
-- RUN: %cc %t.o %t.b.o %gnatstub -o %t
-- RUN: %not %t 2> %t.out
-- RUN: %FileCheck %s < %t.out

-- A division that is an operand of another checked operation: in `(A / B) * C`,
-- a zero divisor fails before the multiplication.

procedure Exec_Divide_Operand is
   R : Integer;

   procedure Scale (A, B, C : Integer) is
   begin
      -- CHECK: raised CONSTRAINT_ERROR : exec_divide_operand.adb:[[#@LINE+1]] divide by zero
      R := (A / B) * C;
   end Scale;
begin
   Scale (100, 5, 3);
   Scale (100, 0, 3);
end Exec_Divide_Operand;
