-- RUN: %lalvm --emit=llvm %s | %llc -filetype=obj -o %t.o
-- RUN: %llvm-dwarfdump --debug-info %t.o | %FileCheck %s

-- `in` parameters (scalar) and `out` parameters (reference) both produce
-- DW_TAG_formal_parameter entries in DWARF, with name and type from NameLoc.
-- An `in` parameter is read-only, so its type is wrapped in DW_TAG_const_type;
-- an `out` (reference) parameter is writable and stays unqualified. The
-- function carries a DW_AT_type for its return type (Integer); the nested
-- procedure has none (void return).

-- CHECK: DW_TAG_subprogram
-- CHECK: DW_AT_type ({{.*}} "integer")
-- CHECK: DW_TAG_formal_parameter
-- CHECK: DW_AT_name ("a")
-- CHECK: DW_AT_type ({{.*}} "const integer")
-- CHECK: DW_TAG_formal_parameter
-- CHECK: DW_AT_name ("b")
-- CHECK: DW_AT_type ({{.*}} "const integer")

-- CHECK: DW_TAG_subprogram
-- CHECK: DW_TAG_formal_parameter
-- CHECK: DW_AT_name ("x")
-- CHECK: DW_AT_type ({{.*}} "integer")

function Param (A, B : Integer) return Integer is
   procedure Get_Value (X : out Integer) is
   begin
      X := 42;
   end Get_Value;
   C : Integer;
begin
   Get_Value (C);
   return A + B + C;
end Param;
