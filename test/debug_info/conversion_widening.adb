-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm -g --emit=llvm %s | %llc -filetype=obj -o %t.o
-- RUN: %llvm-dwarfdump --debug-info %t.o | %FileCheck %s

-- A constant initialized by a widening conversion keeps its Ada type, not
-- another type of the same machine representation (here Integer).

-- CHECK: DW_TAG_variable
-- CHECK: DW_AT_name ("x")
-- CHECK: DW_AT_type ({{.*}} "count")

function Conversion_Widening (S : Short_Integer) return Integer is
   type Count is range -100_000 .. 100_000;
   X : constant Count := Count (S);
begin
   return Integer (X);
end Conversion_Widening;
