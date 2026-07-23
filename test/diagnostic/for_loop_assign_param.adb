-- RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

-- The loop parameter is a constant view (RM 5.5(6)): assignment is rejected
-- even though it is backed by a mutable alloca.

-- CHECK: error: assignment to loop parameter not allowed

function For_Loop_Assign_Param return Integer is
   R : Integer := 0;
begin
   for I in 1 .. 5 loop
      I := I + 1;
      R := R + I;
   end loop;
   return R;
end For_Loop_Assign_Param;
