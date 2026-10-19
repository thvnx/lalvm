-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- REQUIRES: cc
-- RUN: %lalvm --emit=obj %s -o %t.o
-- RUN: %lalvm --bind %s -o %t.b.o
-- RUN: %cc %t.o %t.b.o %gnatstub -o %t
-- RUN: %not %t 2> %t.out
-- RUN: %FileCheck %s < %t.out

-- Subtype bounds are evaluated at elaboration (RM 3.2.2), so `N * 2` overflows.

procedure Exec_Dynamic_Bound_Overflow is
   procedure Fits (N : Integer) is
      subtype S is Integer range 1 .. N * 2;
   begin
      null;
   end Fits;

   procedure Overflows (N : Integer) is
      -- CHECK: raised CONSTRAINT_ERROR : exec_dynamic_bound_overflow.adb:[[#@LINE+1]] overflow check failed
      subtype S is Integer range 1 .. N * 2;
   begin
      null;
   end Overflows;
begin
   Fits (1_000_000_000);
   Overflows (1_500_000_000);
end Exec_Dynamic_Bound_Overflow;
