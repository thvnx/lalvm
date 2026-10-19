-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- REQUIRES: cc
-- RUN: %lalvm --emit=obj %s -o %t.o
-- RUN: %lalvm --bind %s -o %t.b.o
-- RUN: %cc %t.o %t.b.o %gnatstub -o %t
-- RUN: %not %t 2> %t.out
-- RUN: %FileCheck %s < %t.out

-- An accumulation in a reverse for loop (RM 5.5) overflows.

procedure Exec_Overflow_For_Loop is
   Sum : Integer := -2_147_483_000;
begin
   for I in reverse 1 .. 100 loop
      -- CHECK: raised CONSTRAINT_ERROR : exec_overflow_for_loop.adb:[[#@LINE+1]] overflow check failed
      Sum := Sum - I;
   end loop;
end Exec_Overflow_For_Loop;
