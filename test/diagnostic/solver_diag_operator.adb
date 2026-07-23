-- RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

-- The initializer's operands have mismatched types (Integer = Boolean), so no
-- predefined "=" applies (RM 4.5.2). The name-resolution failure is caught at
-- the declaration (the xref entry point) and reported as a headline at the
-- operator plus the type detail at the operand, rather than a misleading error
-- emitted while descending into the unresolved expression.

-- CHECK: solver_diag_operator.adb:15:21: error: no matching candidate for "="
-- CHECK: solver_diag_operator.adb:15:19: error: expected Boolean, got Integer

procedure Solver_Diag_Operator is
   X : Integer := 1;
   Y : Boolean := True;
   Z : Boolean := X = Y;
begin
   null;
end Solver_Diag_Operator;
