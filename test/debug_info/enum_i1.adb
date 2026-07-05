-- RUN: %lalvm -g --emit=llvm %s | %FileCheck %s

-- A 2-literal enum lowers to a DWARF DW_TAG_enumeration_type carrying both
-- enumerators with their representation values.

-- CHECK: !DICompositeType(tag: DW_TAG_enumeration_type, name: "switch"
-- CHECK: !DIEnumerator(name: "off", value: 0)
-- CHECK: !DIEnumerator(name: "on", value: 1)

procedure Enum_I1 is
   type Switch is (Off, On);
   D : constant Switch := On;
begin
   null;
end Enum_I1;
