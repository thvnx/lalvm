-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- REQUIRES: cc
-- RUN: %lalvm --emit=obj %s -o %t.o
-- RUN: %lalvm --bind %s -o %t.b.o
-- RUN: %cc %t.o %t.b.o %gnatstub -o %t
-- RUN: %not %t 2> %t.out
-- RUN: %FileCheck %s < %t.out

-- Narrowing conversion out of range.

procedure Conversion_Narrowing is
   X : Integer := 65_541;
   Y : Short_Integer;
begin
   -- CHECK: raised CONSTRAINT_ERROR : conversion_narrowing.adb:[[#@LINE+1]] range check failed
   Y := Short_Integer (X);
end Conversion_Narrowing;
