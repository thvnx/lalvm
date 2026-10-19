-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- REQUIRES: cc
-- RUN: %lalvm --emit=obj %s -o %t.o
-- RUN: %lalvm --bind %s -o %t.b.o
-- RUN: %cc %t.o %t.b.o %gnatstub -o %t
-- RUN: %not %t 2> %t.out
-- RUN: %FileCheck %s < %t.out

-- Index check on a computed index `V (I + 1)`, one past the last element.

procedure Exec_Index_Computed is
   type Vec is array (5 .. 14) of Integer;
   V    : Vec;
   Item : Integer;

   procedure Read_Next (I : Integer) is
   begin
      Item := V (I + 1);
   end Read_Next;

   procedure Read_Past (I : Integer) is
   begin
      -- CHECK: raised CONSTRAINT_ERROR : exec_index_computed.adb:[[#@LINE+1]] index check failed
      Item := V (I + 1);
   end Read_Past;
begin
   V (13) := 1;
   V (14) := 2;
   Read_Next (12);
   Read_Next (13);
   Read_Past (14);
end Exec_Index_Computed;
