-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- REQUIRES: cc
-- RUN: %lalvm --emit=obj %s -o %t.o
-- RUN: %lalvm --bind %s -o %t.b.o
-- RUN: %cc %t.o %t.b.o %gnatstub -o %t
-- RUN: %not %t 2> %t.out
-- RUN: %FileCheck %s < %t.out

-- An up-level `Total` updated in a nested subprogram keeps its overflow check.

procedure Exec_Nested_Subp_Overflow is
   Total : Integer := 2_147_483_000;

   function Add (N : Integer; Times : Integer) return Integer is
   begin
      if Times = 0 then
         return Total;
      end if;
      -- CHECK: raised CONSTRAINT_ERROR : exec_nested_subp_overflow.adb:[[#@LINE+1]] overflow check failed
      Total := Total + N;
      return Add (N, Times - 1);
   end Add;

   R : Integer;
begin
   R := Add (300, 3);
end Exec_Nested_Subp_Overflow;
