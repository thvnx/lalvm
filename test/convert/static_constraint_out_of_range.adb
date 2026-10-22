-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

-- Static value outside a static subtype.

-- CHECK: %[[C:.*]] = ada.coerce %{{.*}} : <i32, @standard.integer> to <i32, @static_constraint_out_of_range.small>
-- CHECK: ada.range_check %[[C]]

procedure Static_Constraint_Out_Of_Range is
   subtype Small is Integer range 1 .. 10;
   Y : Small := 20;
begin
   null;
end Static_Constraint_Out_Of_Range;
