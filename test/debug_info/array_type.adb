-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project
--
-- An array object gets a DW_TAG_array_type with a typed, bounded subrange.

-- RUN: %lalvm -g --emit=llvm %s | %llc -filetype=obj -o %t.o
-- RUN: %llvm-dwarfdump --debug-info %t.o | %FileCheck %s

-- llvm-dwarfdump renders a reference to an array type as `element[]`.
-- CHECK: DW_TAG_variable
-- CHECK: DW_AT_name ("v")
-- CHECK: DW_AT_type ({{.*}} "integer[]")
-- CHECK: DW_TAG_array_type
-- CHECK: DW_AT_type ({{.*}} "integer")
-- CHECK: DW_AT_name ("vec")
-- CHECK: DW_TAG_subrange_type
-- CHECK: DW_AT_type ({{.*}} "integer")
-- CHECK: DW_AT_lower_bound (5)
-- CHECK: DW_AT_upper_bound (14)

function Array_Type return Integer is
   type Vec is array (5 .. 14) of Integer;
   V : Vec;
begin
   V (5) := 1;
   return V (5);
end Array_Type;
