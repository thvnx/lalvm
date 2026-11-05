-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- REQUIRES: cc
-- RUN: %lalvm -O1 --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm -O1 --emit=llvm %s | %FileCheck %s --check-prefix=LLVM
-- RUN: %lalvm -O1 --emit=obj %s -o %t.o
-- RUN: %lalvm --bind %s -o %t.b.o
-- RUN: %cc %t.o %t.b.o %gnatstub -o %t
-- RUN: %not %t 2> %t.out
-- RUN: %FileCheck %s --check-prefix=EXEC < %t.out

-- Removing unused local objects must preserve checks on their initializers.
-- Parameters keep the arithmetic dynamic when Checked is optimized.
-- MLIR-LABEL: ada.subp private @exec_unused_operator_checks.checked(
-- MLIR: ada.binop "+" {{.*}} checks<overflow>
-- MLIR: ada.binop "/" {{.*}} checks<division>
-- MLIR: ada.unop "-" {{.*}} checks<overflow>
-- MLIR: ada.unop "abs" {{.*}} checks<overflow>
-- LLVM-LABEL: define internal void @exec_unused_operator_checks__checked(
-- LLVM: @llvm.sadd.with.overflow.i32
-- LLVM: @__gnat_rcheck_CE_Overflow_Check
-- LLVM: @__gnat_rcheck_CE_Divide_By_Zero
-- LLVM: @llvm.ssub.with.overflow.i32
-- LLVM: @__gnat_rcheck_CE_Overflow_Check
-- LLVM: @__gnat_rcheck_CE_Overflow_Check
-- LLVM: ret void

procedure Exec_Unused_Operator_Checks is
   procedure Checked (A, B : Integer) is
      -- EXEC: raised CONSTRAINT_ERROR : exec_unused_operator_checks.adb:[[#@LINE+1]] overflow check failed
      Sum      : Integer := A + B;
      Quotient : Integer := A / B;
      Negative : Integer := -A;
      Absolute : Integer := abs A;
   begin
      null;
   end Checked;
begin
   Checked (Integer'Last, 1);
end Exec_Unused_Operator_Checks;
