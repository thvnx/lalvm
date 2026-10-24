-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

-- A static conversion outside the base range of its type is illegal.

-- CHECK: error: value not in range of type "{{.*}}short_integer"
-- CHECK: error: static expression fails Constraint_Check

function Type_Conversion_Static_Out_Of_Range return Short_Integer is
begin
   return Short_Integer (70_000);
end Type_Conversion_Static_Out_Of_Range;
