-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s
-- RUN: %lalvm --emit=llvm --mlir-print-ir-after=ada-lower-conversions %s -o /dev/null 2>&1 | %FileCheck %s --check-prefix=LOWERED

-- A literal flowing into a dynamic-bound subtype still needs a run-time
-- Constraint_Check (RM 11.5): the literal is an `Integer`, converted to `S`
-- when it initializes `V` (RM 3.3.1), `1 <= N` can only be checked at run time.
-- CHECK: ada.convert implicit %{{.*}}, %{{.*}} : !ada.qual<i32, @standard.integer> to !ada.qual<i32, @dynamic_constraint.s>

-- LOWERED: ada.range_check %{{.*}}, %{{.*}} : !ada.qual<i32, @standard.integer> to !ada.qual<i32, @dynamic_constraint.s>

function Dynamic_Constraint (N : Integer) return Integer is
   subtype S is Integer range 1 .. N;
   V : S := 1;
begin
   return V;
end Dynamic_Constraint;
