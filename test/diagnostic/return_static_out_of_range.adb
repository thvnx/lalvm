-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=llvm %s 2>&1 | %FileCheck %s

-- A static value outside the result subtype is legal in a return, it only
-- fails the range check.

-- CHECK: warning: value not in range of type "{{.*}}small"
-- CHECK: warning: Constraint_Error will be raised at run time
-- CHECK: call void @__gnat_rcheck_CE_Range_Check(

function Return_Static_Out_Of_Range return Integer is
   subtype Small is Integer range 1 .. 10;
   function Get return Small is
   begin
      return 20;
   end Get;
begin
   return Get;
end Return_Static_Out_Of_Range;
