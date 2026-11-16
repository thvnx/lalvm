-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

-- Diagnose static division by zero when evaluation fails.
-- CHECK-NOT: failed to evaluate static integer expression
-- CHECK: division_by_zero_static_expr.adb:[[@LINE+4]]:{{[0-9]+}}: error: division by zero
-- CHECK: error: static expression fails Constraint_Check

function Division_By_Zero_Static_Expr return Integer is
   C : constant Integer := 1 / 0;
begin
   return C;
end Division_By_Zero_Static_Expr;
