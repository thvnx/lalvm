-- RUN: %lalvm --emit=llvm %s | %FileCheck %s

-- Division_Check (RM 11.5) and division overflow (RM 4.5). Signed integer `/`
-- is guarded before the divide: a zero divisor raises Constraint_Error via
-- `__gnat_rcheck_CE_Divide_By_Zero`, and `Integer'First / -1` (whose result
-- does not fit the signed range) is prechecked via
-- `__gnat_rcheck_CE_Overflow_Check`. The signed divide follows the prechecks.

-- CHECK-LABEL: define i32 @_ada_division_check
-- CHECK:         icmp eq i32 %{{.*}}, 0
-- CHECK:         call void @__gnat_rcheck_CE_Divide_By_Zero
-- CHECK:         icmp eq i32 %{{.*}}, -2147483648
-- CHECK:         icmp eq i32 %{{.*}}, -1
-- CHECK:         and i1
-- CHECK:         call void @__gnat_rcheck_CE_Overflow_Check
-- CHECK:         sdiv i32

function Division_Check (A, B : Integer) return Integer is
begin
   return A / B;
end Division_Check;
