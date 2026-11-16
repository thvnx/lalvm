-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

-- Diagnose a static zero divisor referenced through a constant.
-- CHECK-NOT: failed to evaluate static integer expression
-- CHECK: mod_by_zero_static.adb:[[@LINE+5]]:{{[0-9]+}}: error: division by zero
-- CHECK: error: static expression fails Constraint_Check

function Mod_By_Zero_Static return Integer is
   Zero : constant Integer := 0;
   C    : constant Integer := 7 mod Zero;
begin
   return C;
end Mod_By_Zero_Static;
