-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- REQUIRES: cc
-- RUN: %lalvm --emit=obj %s -o %t.o
-- RUN: %lalvm --bind %s -o %t.b.o
-- RUN: %cc %t.o %t.b.o %gnatstub -o %t
-- RUN: %not %t 2> %t.out
-- RUN: %FileCheck %s < %t.out

-- A returned value outside the result subtype fails the range check of the
-- return statement, in the callee.

procedure Exec_Return_Range is
   subtype Small is Integer range 1 .. 10;
   R : Integer;

   function To_Small (X : Integer) return Small is
   begin
      -- CHECK: raised CONSTRAINT_ERROR : exec_return_range.adb:[[#@LINE+1]] range check failed
      return X;
   end To_Small;
begin
   R := To_Small (10);
   R := To_Small (20);
end Exec_Return_Range;
