-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=llvm %s 2>&1 | %FileCheck %s

-- A static value outside the target's subtype is legal in an assignment, it
-- only fails the range check.

-- CHECK: warning: value not in range of type "{{.*}}small"
-- CHECK: warning: Constraint_Error will be raised at run time
-- CHECK: call void @__gnat_rcheck_CE_Range_Check(

procedure Assignment_Static_Out_Of_Range is
   subtype Small is Integer range 1 .. 10;
   Y : Small := 1;
begin
   Y := 20;
end Assignment_Static_Out_Of_Range;
