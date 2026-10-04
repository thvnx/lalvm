-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- REQUIRES: cc
-- RUN: %lalvm --emit=obj %s -o %t.o
-- RUN: %lalvm --bind %s -o %t.b.o
-- RUN: %cc %t.o %t.b.o %gnatstub -o %t
-- RUN: %not %t 2> %t.out
-- RUN: %FileCheck %s < %t.out

-- An index outside the array's bounds fails the index check at run time.

-- CHECK: raised CONSTRAINT_ERROR : exec_index_check.adb:22 index check failed
procedure Exec_Index_Check is
   type Vec is array (1 .. 10) of Integer;
   V : Vec;
   I : Integer := 5;
begin
   V (I) := 1;
   I := 20;
   V (1) := 2;
   V (I) := 3;
end Exec_Index_Check;
