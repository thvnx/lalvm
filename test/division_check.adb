-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.subp @division_check
-- MLIR:         %[[R:.*]] = ada.binop "/" %a, %b checks<division> : (!ada.qual<i32, @standard.integer>, !ada.qual<i32, @standard.integer>) -> !ada.qual<i32, @standard.integer>
-- MLIR-NEXT:    ada.return %[[R]] : !ada.qual<i32, @standard.integer>

-- Division_Check (RM 11.5) and division overflow (RM 4.5). Signed integer `/`
-- is guarded before the divide: a zero divisor raises Constraint_Error via
-- `__gnat_rcheck_CE_Divide_By_Zero`, and `Integer'First / -1` (whose result
-- does not fit the signed range) is prechecked via
-- `__gnat_rcheck_CE_Overflow_Check`. The signed divide follows the prechecks.

-- LLVM-LABEL: define i32 @_ada_division_check
-- LLVM:         icmp eq i32 %{{.*}}, 0
-- LLVM:         call void @__gnat_rcheck_CE_Divide_By_Zero
-- LLVM:         icmp eq i32 %{{.*}}, -2147483648
-- LLVM:         icmp eq i32 %{{.*}}, -1
-- LLVM:         and i1
-- LLVM:         call void @__gnat_rcheck_CE_Overflow_Check
-- LLVM:         %[[DIV:.*]] = sdiv i32 %0, %1
-- LLVM-NEXT:    ret i32 %[[DIV]]

function Division_Check (A, B : Integer) return Integer is
begin
   return A / B;
end Division_Check;
