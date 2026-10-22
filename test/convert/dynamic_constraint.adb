-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- Dynamic subtype: initializer, bound of T, and S'Last.

-- CHECK: %[[C:.*]] = ada.coerce %arg1 : <i32, @standard.integer> to <i32, @dynamic_constraint.s>
-- CHECK: ada.range_check %[[C]]
-- CHECK: ada.coerce %{{.*}} : <i32, @dynamic_constraint.s> to <i32, @standard.integer>
-- CHECK: ada.range %{{.*}} -> !ada.range<i32, @dynamic_constraint.t>
-- CHECK: ada.attr "last"
-- CHECK: ada.coerce %{{.*}} : <i32, @dynamic_constraint.s> to <i32, @standard.integer>

function Dynamic_Constraint (N, M : Integer) return Integer is
   subtype S is Integer range 1 .. N;
   X : S := M;
   subtype T is Integer range 1 .. X;
begin
   return S'Last;
end Dynamic_Constraint;
