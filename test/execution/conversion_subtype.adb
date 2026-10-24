-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- REQUIRES: cc
-- RUN: %lalvm --emit=obj %s -o %t.o
-- RUN: %lalvm --bind %s -o %t.b.o
-- RUN: %cc %t.o %t.b.o %gnatstub -o %t
-- RUN: %not %t 2> %t.out
-- RUN: %FileCheck %s < %t.out

-- Conversion to a subtype out of range.

procedure Conversion_Subtype is
   subtype Small is Integer range 1 .. 10;
   X : Integer := 11;
   Y : Integer;
begin
   -- CHECK: raised CONSTRAINT_ERROR : conversion_subtype.adb:[[#@LINE+1]] range check failed
   Y := Small (X);
end Conversion_Subtype;
