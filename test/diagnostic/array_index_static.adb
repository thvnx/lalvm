-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=llvm %s 2>&1 | %FileCheck %s

-- A static index outside the array's bounds is legal but fails the index check:
-- warn and raise at run time.

-- CHECK: warning: value not in range of type
-- CHECK: warning: Constraint_Error will be raised at run time
-- CHECK: call void @__gnat_rcheck_CE_Index_Check(

procedure Array_Index_Static is
   type Vec is array (1 .. 10) of Integer;
   V : Vec;
   X : Integer;
begin
   X := V (20);
end Array_Index_Static;
