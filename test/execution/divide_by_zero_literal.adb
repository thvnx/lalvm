-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- REQUIRES: cc
-- RUN: %lalvm --emit=mlir %s 2> %t.diag | %FileCheck %s --check-prefix=MLIR
-- RUN: %FileCheck %s --check-prefix=WARN < %t.diag
-- RUN: %lalvm --bind %s -o %t.bind.o
-- RUN: %lalvm -O0 --emit=obj %s -o %t.o
-- RUN: %cc %t.o %t.bind.o %gnatstub -o %t
-- RUN: %not %t 2> %t.err
-- RUN: %FileCheck %s --check-prefix=EXEC < %t.err
-- RUN: %lalvm -O1 --emit=obj %s -o %t.opt.o
-- RUN: %cc %t.opt.o %t.bind.o %gnatstub -o %t.opt
-- RUN: %not %t.opt 2> %t.err
-- RUN: %FileCheck %s --check-prefix=EXEC < %t.err

-- Nonstatic divisions with literal zero divisors warn and keep their checks.
-- MLIR-LABEL: ada.subp private @divide_by_zero_literal.f(
-- MLIR: %[[ZERO:.*]] = ada.constant : !ada.qual<i32, @standard.integer> = 0
-- MLIR-NEXT: ada.binop "/" %x, %[[ZERO]] checks<division>
-- MLIR-LABEL: ada.subp private @divide_by_zero_literal.g(
-- MLIR: %[[ZERO:.*]] = ada.constant : !ada.qual<i8, @divide_by_zero_literal.m> = 0
-- MLIR-NEXT: ada.binop "/" %y, %[[ZERO]] checks<division>

procedure Divide_By_Zero_Literal is
   function F (X : Integer) return Integer is
   begin
      -- WARN: divide_by_zero_literal.adb:[[#@LINE+3]]:18: warning: division by zero
      -- WARN-NEXT: divide_by_zero_literal.adb:[[#@LINE+2]]:18: warning: Constraint_Error will be raised at run time
      -- EXEC: raised CONSTRAINT_ERROR : divide_by_zero_literal.adb:[[#@LINE+1]] divide by zero
      return X / 0;
   end F;

   type M is mod 8;
   function G (Y : M) return M is
   begin
      -- WARN: divide_by_zero_literal.adb:[[#@LINE+2]]:18: warning: division by zero
      -- WARN-NEXT: divide_by_zero_literal.adb:[[#@LINE+1]]:18: warning: Constraint_Error will be raised at run time
      return Y / 0;
   end G;

   N : Integer := 1;
begin
   N := F (N);
end Divide_By_Zero_Literal;
