-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s 2>&1 | %FileCheck %s
-- RUN: %lalvm --emit=llvm --mlir-print-ir-after=ada-lower-conversions %s -o /dev/null 2>&1 | %FileCheck %s --check-prefix=LOWERED

-- Static value outside a static subtype.

-- CHECK: warning: value not in range of type
-- CHECK: ada.convert implicit %{{.*}} : !ada.qual<i32, @standard.integer> to !ada.qual<i32, @static_constraint_out_of_range.small>

-- LOWERED: ada.range_check %{{.*}}, %{{.*}} : !ada.qual<i32, @standard.integer> to !ada.qual<i32, @static_constraint_out_of_range.small>

procedure Static_Constraint_Out_Of_Range is
   subtype Small is Integer range 1 .. 10;
   Y : Small := 20;
begin
   null;
end Static_Constraint_Out_Of_Range;
