-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- REQUIRES: cc
-- RUN: %lalvm --emit=obj %s -o %t.o
-- RUN: %lalvm --bind %s -o %t.b.o
-- RUN: %cc %t.o %t.b.o %gnatstub -o %t
-- RUN: %not %t 2> %t.out
-- RUN: %FileCheck %s < %t.out
-- RUN: %lalvm -O1 --emit=obj %s -o %t.opt.o
-- RUN: %cc %t.opt.o %t.b.o %gnatstub -o %t.opt
-- RUN: %not %t.opt 2> %t.opt.out
-- RUN: %FileCheck %s < %t.opt.out

-- Check the results of successful chains, then overflow the addition in
-- `(A / B) + C` after its division passes. An incorrect result returns
-- normally, which fails the expected error exit.

procedure Exec_Check_Chains is
   subtype Small is Integer range 1 .. 10;
   type Vec is array (1 .. 10) of Integer;
   V : Vec;
   R : Integer;
   S : Small;

   function Succ (X : Integer) return Small is
   begin
      return X + 1;
   end Succ;

   procedure Set (A, B : Integer) is
   begin
      S := A / B;
   end Set;

   procedure Read_Next (I : Integer) is
   begin
      R := V (I + 1);
   end Read_Next;

   procedure Shift (A, B, C : Integer) is
   begin
      -- CHECK: raised CONSTRAINT_ERROR : exec_check_chains.adb:[[#@LINE+1]] overflow check failed
      R := (A / B) + C;
   end Shift;
begin
   V (10) := 1;
   R := Succ (9);
   if R /= 10 then
      return;
   end if;
   Set (100, 10);
   if S /= 10 then
      return;
   end if;
   Read_Next (9);
   if R /= 1 then
      return;
   end if;
   Shift (100, 5, 3);
   if R /= 23 then
      return;
   end if;
   Shift (Integer'Last, 1, 1);
end Exec_Check_Chains;
