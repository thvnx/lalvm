-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

-- A static division by zero is illegal (RM 4.9(34)). A modular one is not
-- folded, so it reaches the division.
-- CHECK: division_by_zero_static.adb:[[@LINE+5]]:17: error: division by zero
-- CHECK: division_by_zero_static.adb:[[@LINE+4]]:17: error: static expression fails Constraint_Check

function Division_By_Zero_Static return Integer is
   type M is mod 8;
   A : M := 1 / 0;
begin
   return Integer (A);
end Division_By_Zero_Static;
