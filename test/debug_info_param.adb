-- RUN: %lalvm --emit=llvm %s | %llc -filetype=obj -o %t.o
-- RUN: %llvm-dwarfdump --debug-info %t.o | %FileCheck %s

-- `in` parameters (scalar) and `out` parameters (reference) both produce
-- DW_TAG_formal_parameter entries in DWARF, with name and type from NameLoc
-- and FusedLoc metadata.

-- CHECK: DW_TAG_subprogram
-- CHECK: DW_TAG_formal_parameter
-- CHECK: DW_AT_name ("a")
-- CHECK: DW_AT_type ({{.*}} "standard.integer")
-- CHECK: DW_TAG_formal_parameter
-- CHECK: DW_AT_name ("b")
-- CHECK: DW_AT_type ({{.*}} "standard.integer")

-- CHECK: DW_TAG_subprogram
-- CHECK: DW_TAG_formal_parameter
-- CHECK: DW_AT_name ("x")
-- CHECK: DW_AT_type ({{.*}} "standard.integer")

function Test_Param_Debug (A, B : Integer) return Integer is
   procedure Get_Value (X : out Integer) is
   begin
      X := 42;
   end Get_Value;
   C : Integer;
begin
   Get_Value (C);
   return A + B + C;
end Test_Param_Debug;
