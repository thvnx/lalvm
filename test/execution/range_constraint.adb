-- REQUIRES: cc
-- RUN: %lalvm --emit=obj %s -o %t.o
-- RUN: %lalvm --bind %s -o %t.b.o
-- RUN: %cc %t.o %t.b.o %gnatstub -o %t
-- RUN: %not %t > %t.out 2>&1
-- RUN: %FileCheck %s < %t.out

-- V = 20 assigned to Small (1 .. 10) fails the range check at run time.
procedure Range_Constraint is
   subtype Small is Integer range 1 .. 10;
   V : Integer := 20;
   -- CHECK: raised CONSTRAINT_ERROR : range_constraint.adb:[[#@LINE+1]] range check failed
   X : Small := V;
begin
   null;
end Range_Constraint;
