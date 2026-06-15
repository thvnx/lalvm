-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- A literal flowing into a dynamic-bound subtype still needs a run-time
-- Constraint_Check (RM 11.5): `1` is nominally of `S`, but `1 <= N` can only be
-- checked at run time. The literal acquires type `S` from context, so `coerce`
-- must not treat it as already constrained and skip the check.
-- CHECK: ada.range_check %{{.*}}, %{{.*}} : !ada.qual<i32, @dynamic_constraint.s>

function Dynamic_Constraint (N : Integer) return Integer is
   subtype S is Integer range 1 .. N;
   V : S := 1;
begin
   return V;
end Dynamic_Constraint;
