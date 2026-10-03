-- REQUIRES: cc
-- RUN: %lalvm --emit=obj %s -o %t.o
-- RUN: %lalvm --bind %s -o %t.b.o
-- RUN: %cc %t.o %t.b.o %gnatstub -o %t
-- RUN: %not %t 2> %t.out
-- RUN: %FileCheck %s < %t.out

-- An object with a dynamic constraint declared in a block that is not in the
-- entry block of its subprogram (here, under an `if`): its range is elaborated
-- there, and V = 20 must still fail the range check.

-- CHECK: raised CONSTRAINT_ERROR : exec_block_dynamic_constraint.adb:22 range check failed
procedure Exec_Block_Dynamic_Constraint is
   function Ten return Integer is (10);
   V : Integer := 20;
begin
   if V > 0 then
      declare
         X : Integer range 1 .. Ten := 1;
      begin
         X := 5;
         X := V;
      end;
   end if;
end Exec_Block_Dynamic_Constraint;
