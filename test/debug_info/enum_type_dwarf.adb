-- RUN: %lalvm --emit=llvm %s | %llc -filetype=obj -o %t.o
-- RUN: %llvm-dwarfdump --debug-info %t.o | %FileCheck %s

-- 3-literal enum: verify DW_TAG_enumeration_type and its enumerators appear
-- in the DWARF debug info with correct names and representation values.

-- CHECK: DW_TAG_enumeration_type
-- CHECK:   DW_AT_name ("color")
-- CHECK:   DW_AT_byte_size (0x01)
-- CHECK:   DW_TAG_enumerator
-- CHECK:     DW_AT_name ("red")
-- CHECK:     DW_AT_const_value (0)
-- CHECK:   DW_TAG_enumerator
-- CHECK:     DW_AT_name ("green")
-- CHECK:     DW_AT_const_value (1)
-- CHECK:   DW_TAG_enumerator
-- CHECK:     DW_AT_name ("blue")
-- CHECK:     DW_AT_const_value (2)

procedure Enum_Type_Dwarf is
   type Color is (Red, Green, Blue);
   C : constant Color := Green;
begin
   null;
end Enum_Type_Dwarf;
