-- RUN: %lalvm -g --emit=llvm %s | %llc -filetype=obj -o %t.o
-- RUN: %llvm-dwarfdump --debug-info %t.o | %FileCheck %s

-- An unconstrained enum subtype is a pure renaming with no own metadata: its
-- debug info delegates to the base, typing `X` by the `boolean` enumeration.

-- CHECK: DW_TAG_enumeration_type
-- CHECK: DW_AT_name ("boolean")
-- CHECK: DW_TAG_variable
-- CHECK: DW_AT_name ("x")
-- CHECK: DW_AT_type ({{.*}} "boolean")

procedure Enum_Subtype is
   subtype B is Boolean;
   procedure Init (V : out B) is
   begin
      V := False;
   end Init;
   X : B;
begin
   Init (X);
end Enum_Subtype;
