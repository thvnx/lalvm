-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- REQUIRES: cc
-- RUN: %lalvm --emit=obj %s -o %t.o
-- RUN: %lalvm --bind %s -o %t.b.o
-- RUN: %cc %t.o %t.b.o %gnatstub -o %t
-- RUN: %not %t 2> %t.out
-- RUN: %FileCheck %s < %t.out

-- A loop made of a backward `goto` (RM 5.8) overflows.

procedure Exec_Overflow_Goto is
   X : Integer := 2_147_483_000;
begin
   <<Top>>
   -- CHECK: raised CONSTRAINT_ERROR : exec_overflow_goto.adb:[[#@LINE+1]] overflow check failed
   X := X + 100;
   if X > 0 then
      goto Top;
   end if;
end Exec_Overflow_Goto;
