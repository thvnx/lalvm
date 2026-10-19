-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- REQUIRES: cc
-- RUN: %lalvm --emit=obj %s -o %t.o
-- RUN: %lalvm --bind %s -o %t.b.o
-- RUN: %cc %t.o %t.b.o %gnatstub -o %t
-- RUN: %not %t 2> %t.out
-- RUN: %FileCheck %s < %t.out

-- Range check of an object of a static subtype declared in a block in a loop.

procedure Exec_Loop_Block_Range is
   I : Integer := 995;
begin
   while I < 2000 loop
      declare
         subtype Step_Range is Integer range 0 .. 1000;
         -- CHECK: raised CONSTRAINT_ERROR : exec_loop_block_range.adb:[[#@LINE+1]] range check failed
         Step : Step_Range := I + 1;
      begin
         I := Step;
      end;
   end loop;
end Exec_Loop_Block_Range;
