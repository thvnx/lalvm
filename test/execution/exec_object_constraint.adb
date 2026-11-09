-- REQUIRES: cc
-- RUN: %lalvm --emit=obj %s -o %t.o
-- RUN: %lalvm --bind %s -o %t.b.o
-- RUN: %cc %t.o %t.b.o %gnatstub -o %t
-- RUN: %not %t 2> %t.out
-- RUN: %FileCheck %s < %t.out

-- The constraint of an object declaration is checked at run time: V = 20 fails
-- the range check.

procedure Exec_Object_Constraint is
   V : Integer := 20;
   X : Integer range 1 .. 10 := 1;
begin
   X := 5;
   -- CHECK: raised CONSTRAINT_ERROR : exec_object_constraint.adb:[[#@LINE+1]] range check failed
   X := V;
end Exec_Object_Constraint;
