-- RUN: %lalvm --emit=llvm %s | %FileCheck %s

-- A 3-literal enum lowers to a DWARF DW_TAG_enumeration_type carrying all
-- three enumerators with their representation values.

-- CHECK: !DICompositeType(tag: DW_TAG_enumeration_type, name: "color"
-- CHECK: !DIEnumerator(name: "red", value: 0)
-- CHECK: !DIEnumerator(name: "green", value: 1)
-- CHECK: !DIEnumerator(name: "blue", value: 2)

procedure Enum_I3 is
   type Color is (Red, Green, Blue);
   C  : constant Color := Green;
   C2 : constant Color := Blue;
begin
   null;
end Enum_I3;
