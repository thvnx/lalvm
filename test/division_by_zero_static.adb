-- RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

-- A static zero divisor fails the Division_Check at compile time (RM 11.5),
-- GNAT-style, rather than emitting a run-time raise.
-- CHECK: error: division by zero
-- CHECK: error: static expression fails Constraint_Check

function Division_By_Zero_Static (A : Integer) return Integer is
begin
   return A / 0;
end Division_By_Zero_Static;
