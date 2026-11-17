-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- REQUIRES: cc
-- RUN: %lalvm --emit=obj -O0 %s -o %t.o
-- RUN: %lalvm --bind %s -o %t.b.o
-- RUN: %cc %t.o %t.b.o %gnatstub -lm -o %t
-- RUN: %not %t 2> %t.out
-- RUN: %FileCheck %s < %t.out
-- RUN: %lalvm --emit=obj -O1 %s -o %t.o
-- RUN: %cc %t.o %t.b.o %gnatstub -lm -o %t
-- RUN: %not %t 2> %t.out
-- RUN: %FileCheck %s < %t.out

-- The rounded value fits Integer's base range but exceeds Small's constraint.

procedure Exec_Conversion_Subtype is
   subtype Small is Integer range 1 .. 10;
   F : Float := 10.5;
   X : Small;
begin
   -- CHECK: raised CONSTRAINT_ERROR : exec_conversion_subtype.adb:[[#@LINE+1]] range check failed
   X := Small (F);
end Exec_Conversion_Subtype;
