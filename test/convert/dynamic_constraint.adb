-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s
-- RUN: %lalvm --emit=llvm --mlir-print-ir-after=ada-lower-conversions %s -o /dev/null 2>&1 | %FileCheck %s --check-prefix=LOWERED

-- Dynamic subtype: initializer, bound of T, and S'Last.

-- CHECK: ada.convert implicit %m, %{{.*}} : !ada.qual<i32, @standard.integer> to !ada.qual<i32, @dynamic_constraint.s>
-- CHECK: ada.convert implicit %{{.*}} : !ada.qual<i32, @dynamic_constraint.s> to !ada.qual<i32, @standard.integer>
-- CHECK: ada.range %{{.*}} -> !ada.range<i32, @dynamic_constraint.t>
-- CHECK: ada.attr "last"
-- CHECK: ada.convert implicit %{{.*}} : !ada.qual<i32, @dynamic_constraint.s> to !ada.qual<i32, @standard.integer>

-- LOWERED: ada.range_check %m, %{{.*}} : !ada.qual<i32, @standard.integer> to !ada.qual<i32, @dynamic_constraint.s>

function Dynamic_Constraint (N, M : Integer) return Integer is
   subtype S is Integer range 1 .. N;
   X : S := M;
   subtype T is Integer range 1 .. X;
begin
   return S'Last;
end Dynamic_Constraint;
