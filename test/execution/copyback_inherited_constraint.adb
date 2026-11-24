-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- REQUIRES: cc
-- RUN: %lalvm --emit=obj %s -o %t.o
-- RUN: %lalvm --bind %s -o %t.b.o
-- RUN: %cc %t.o %t.b.o %gnatstub -o %t
-- RUN: %not %t 2>&1 | %FileCheck %s

procedure Copyback_Inherited_Constraint is
   subtype S is Positive;
   subtype S2 is S;
   X : S2 := 1;

   procedure Set_Zero (Y : out Integer) is
   begin
      Y := 0;
   end Set_Zero;
begin
   -- CHECK: raised CONSTRAINT_ERROR : copyback_inherited_constraint.adb:[[#@LINE+1]] range check failed
   Set_Zero (X);
end Copyback_Inherited_Constraint;
