-- RUN: %lalvm --emit=llvm %s | %llc -filetype=obj -o %t.o
-- RUN: %llvm-dwarfdump --debug-info %t.o | %FileCheck %s

-- Modular-typed local variables produce DW_TAG_variable entries in DWARF,
-- with DW_ATE_unsigned encoding and the Ada fully-qualified type name.

-- CHECK: DW_TAG_subprogram
-- CHECK: DW_TAG_variable
-- CHECK: DW_AT_name ("x")
-- CHECK: DW_AT_type ({{.*}} "t")
-- CHECK: DW_TAG_base_type
-- CHECK: DW_AT_name ("t")
-- CHECK: DW_AT_encoding (DW_ATE_unsigned)
-- CHECK: DW_AT_byte_size (0x01)

procedure Debug_Info_Mod is
   type T is mod 128;
   procedure Init (V : out T) is
   begin
      V := 0;
   end Init;
   X : T;
begin
   Init (X);
end Debug_Info_Mod;
