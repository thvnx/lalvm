-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=llvm %s 2>&1 | %FileCheck %s

-- The constraint of an object declaration applies to its initializer: a static
-- value outside it is legal but fails at run time.

-- CHECK: warning: value not in range of type
-- CHECK: warning: Constraint_Error will be raised at run time
-- CHECK: call void @__gnat_rcheck_CE_Range_Check(

procedure Object_Constraint_Static is
   X : Integer range 1 .. 10 := 20;
begin
   null;
end Object_Constraint_Static;
