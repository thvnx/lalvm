-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- REQUIRES: cc
-- RUN: %lalvm --emit=obj %s -o %t.o
-- RUN: %lalvm --bind %s -o %t.b.o
-- RUN: %cc %t.o %t.b.o %gnatstub -o %t
-- RUN: %not %t 2> %t.out
-- RUN: %FileCheck %s < %t.out

-- Index check on a loop parameter: the second loop fails at 6.

procedure Exec_Index_Loop is
   type Vec is array (1 .. 5) of Integer;
   V : Vec;
begin
   for I in 1 .. 5 loop
      V (I) := I;
   end loop;
   for I in 5 .. 6 loop
      -- CHECK: raised CONSTRAINT_ERROR : exec_index_loop.adb:[[#@LINE+1]] index check failed
      V (I) := 0;
   end loop;
end Exec_Index_Loop;
