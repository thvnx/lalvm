-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- REQUIRES: cc
-- RUN: %lalvm --emit=obj %s -o %t.o
-- RUN: %lalvm --bind %s -o %t.b.o
-- RUN: %cc %t.o %t.b.o %gnatstub -o %t
-- RUN: %not %t 2> %t.out
-- RUN: %FileCheck %s < %t.out

-- Narrowing conversion to a dynamic subtype, out of its base range.

procedure Conversion_Dynamic is
   procedure Run (N : Integer) is
      subtype S is Integer range 1 .. N;
      L : Long_Integer := 2**32 + 5;
      X : Integer;
   begin
      -- CHECK: raised CONSTRAINT_ERROR : conversion_dynamic.adb:[[#@LINE+1]] range check failed
      X := S (L);
   end Run;
begin
   Run (10);
end Conversion_Dynamic;
