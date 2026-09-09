-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project
--
-- An array of an enum reuses the enum's DW_TAG_enumeration_type as element
-- type; the enum gets its DIE through the array alone.

-- RUN: %lalvm -g --emit=llvm %s | %llc -filetype=obj -o %t.o
-- RUN: %llvm-dwarfdump --debug-info %t.o | %FileCheck %s

-- CHECK: DW_TAG_enumeration_type
-- CHECK: DW_AT_name ("boolean")
-- CHECK: DW_TAG_enumerator
-- CHECK: DW_AT_name ("false")
-- CHECK: DW_TAG_enumerator
-- CHECK: DW_AT_name ("true")
-- CHECK: DW_TAG_variable
-- CHECK: DW_AT_name ("f")
-- CHECK: DW_AT_type ({{.*}} "boolean[]")
-- CHECK: DW_TAG_array_type
-- CHECK: DW_AT_type ({{.*}} "boolean")
-- CHECK: DW_AT_name ("flags")
-- CHECK: DW_TAG_subrange_type
-- CHECK: DW_AT_type ({{.*}} "integer")
-- CHECK: DW_AT_upper_bound (4)

function Array_Enum return Integer is
   type Flags is array (1 .. 4) of Boolean;
   F : Flags;
begin
   F (2) := True;
   return 0;
end Array_Enum;
