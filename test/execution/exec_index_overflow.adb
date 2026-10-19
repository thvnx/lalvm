-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- REQUIRES: cc
-- RUN: %lalvm --emit=obj %s -o %t.o
-- RUN: %lalvm --bind %s -o %t.b.o
-- RUN: %cc %t.o %t.b.o %gnatstub -o %t
-- RUN: %not %t 2> %t.out
-- RUN: %FileCheck %s < %t.out

-- An overflowing index expression: in `V (I * 2)`, the overflow check fails
-- before the index check.

procedure Exec_Index_Overflow is
   type Vec is array (1 .. 10) of Integer;
   V    : Vec;
   Item : Integer;

   procedure Read_Double (I : Integer) is
   begin
      -- CHECK: raised CONSTRAINT_ERROR : exec_index_overflow.adb:[[#@LINE+1]] overflow check failed
      Item := V (I * 2);
   end Read_Double;
begin
   V (10) := 1;
   Read_Double (5);
   Read_Double (2 ** 30);
end Exec_Index_Overflow;
