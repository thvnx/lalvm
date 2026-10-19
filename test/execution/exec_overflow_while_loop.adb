-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- REQUIRES: cc
-- RUN: %lalvm --emit=obj %s -o %t.o
-- RUN: %lalvm --bind %s -o %t.b.o
-- RUN: %cc %t.o %t.b.o %gnatstub -o %t
-- RUN: %not %t 2> %t.out
-- RUN: %FileCheck %s < %t.out

-- An accumulation in a while loop (RM 5.5) overflows.

procedure Exec_Overflow_While_Loop is
   Sum : Integer := 2_147_483_600;
   I   : Integer := 1;
begin
   while I <= 100 loop
      -- CHECK: raised CONSTRAINT_ERROR : exec_overflow_while_loop.adb:[[#@LINE+1]] overflow check failed
      Sum := Sum + I;
      I := I + 1;
   end loop;
end Exec_Overflow_While_Loop;
