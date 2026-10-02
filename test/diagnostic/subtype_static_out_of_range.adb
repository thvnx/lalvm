-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=llvm %s 2>&1 | %FileCheck %s

-- A static value inside the base range but outside a subtype's range is legal,
-- it only fails the range check (RM 4.9(35), 11.5).

-- CHECK: warning: value not in range of type "{{.*}}small"
-- CHECK: warning: Constraint_Error will be raised at run time
-- CHECK: call void @__gnat_rcheck_CE_Range_Check(

procedure Subtype_Static_Out_Of_Range is
   subtype Small is Integer range 1 .. 10;
   X : Small := 20;
begin
   null;
end Subtype_Static_Out_Of_Range;
