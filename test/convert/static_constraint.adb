-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- Static subtype constraint, on an object initializer.

-- CHECK: %[[C:.*]] = ada.coerce %arg0 : <i32, @standard.integer> to <i32, @static_constraint.small>
-- CHECK: ada.range_check %[[C]]

procedure Static_Constraint (X : Integer) is
   subtype Small is Integer range 1 .. 10;
   Y : Small := X;
begin
   null;
end Static_Constraint;
