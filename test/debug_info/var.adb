-- RUN: %lalvm --emit=llvm %s | %llc -filetype=obj -o %t.o
-- RUN: %llvm-dwarfdump --debug-info %t.o | %FileCheck %s

-- Integer and float local variables produce DW_TAG_variable entries in DWARF,
-- with the Ada fully-qualified type name (not the generic "integer_N" fallback).

-- CHECK: DW_TAG_subprogram
-- CHECK: DW_TAG_variable
-- CHECK: DW_AT_name ("i")
-- CHECK: DW_AT_type ({{.*}} "integer")
-- CHECK: DW_TAG_variable
-- CHECK: DW_AT_name ("f")
-- CHECK: DW_AT_type ({{.*}} "float")
-- CHECK: DW_TAG_base_type
-- CHECK: DW_AT_name ("integer")
-- CHECK: DW_AT_encoding (DW_ATE_signed)
-- CHECK: DW_AT_byte_size (0x04)
-- CHECK: DW_TAG_base_type
-- CHECK: DW_AT_name ("float")
-- CHECK: DW_AT_encoding (DW_ATE_float)
-- CHECK: DW_AT_byte_size (0x04)

function Var return Integer is
   procedure Get_Int (X : out Integer) is
   begin
      X := 42;
   end Get_Int;
   procedure Get_Float (X : out Float) is
   begin
      X := 3.14;
   end Get_Float;
   I : Integer;
   F : Float;
begin
   Get_Int (I);
   Get_Float (F);
   return I;
end Var;
