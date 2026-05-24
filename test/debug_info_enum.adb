-- RUN: %lalvm --emit=llvm %s | %llc -filetype=obj -o %t.o
-- RUN: %llvm-dwarfdump --debug-info %t.o | %FileCheck %s

-- Enum-typed `in` parameters, `out` parameters, and local variables all
-- produce proper DW_TAG_formal_parameter / DW_TAG_variable entries with
-- DW_AT_type pointing to a full DW_TAG_enumeration_type (with enumerator
-- names), built by attachAdaDebugInfo after FinalizeAdaObjectPass emits
-- the placeholder DIBasicType.

-- CHECK: DW_TAG_enumeration_type
-- CHECK: DW_AT_name ("standard.boolean")
-- CHECK: DW_TAG_enumerator
-- CHECK: DW_AT_name ("false")
-- CHECK: DW_AT_const_value (0)
-- CHECK: DW_TAG_enumerator
-- CHECK: DW_AT_name ("true")
-- CHECK: DW_AT_const_value (1)
-- CHECK: DW_TAG_subprogram
-- CHECK: DW_TAG_formal_parameter
-- CHECK: DW_AT_name ("a")
-- CHECK: DW_AT_type ({{.*}} "standard.boolean")
-- CHECK: DW_TAG_variable
-- CHECK: DW_AT_name ("b")
-- CHECK: DW_AT_type ({{.*}} "standard.boolean")
-- CHECK: DW_TAG_subprogram
-- CHECK: DW_TAG_formal_parameter
-- CHECK: DW_AT_name ("x")
-- CHECK: DW_AT_type ({{.*}} "standard.boolean")

function Test_Enum_Debug (A : Boolean) return Integer is
   procedure Reset (X : out Boolean) is
   begin
      X := False;
   end Reset;
   B : Boolean;
begin
   Reset (B);
   return 0;
end Test_Enum_Debug;
