-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- REQUIRES: cc
-- RUN: %lalvm --emit=obj %s -o %t.o
-- RUN: %lalvm --bind %s -o %t.b.o
-- RUN: %cc %t.o %t.b.o %gnatstub -o %t
-- RUN: %not %t 2> %t.out
-- RUN: %FileCheck %s < %t.out

-- Integer'First / -1 overflows: the division raises Constraint_Error at run time.

procedure Exec_Divide_Overflow is
   A : Integer := Integer'First;
   B : Integer := -1;
   -- CHECK: raised CONSTRAINT_ERROR : exec_divide_overflow.adb:[[#@LINE+1]] overflow check failed
   C : Integer := A / B;
begin
   null;
end Exec_Divide_Overflow;
