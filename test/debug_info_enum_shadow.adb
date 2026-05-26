-- RUN: %lalvm --emit=llvm %s | %FileCheck %s

-- Three i8 enum types at the same scope. DILocalVariable for "green_color"
-- must reference the color DICompositeType, not direction or flag (which
-- share the same i8 machine type and would be picked by a type-keyed cache).

-- CHECK-DAG: ![[COLOR:[0-9]+]] = !DICompositeType(tag: DW_TAG_enumeration_type, name: "color"
-- CHECK-DAG: !DILocalVariable(name: "green_color", scope: {{.*}}, file: {{.*}}, line: 14, type: ![[COLOR]])

procedure Debug_Info_Enum_Shadow is
   type Direction is (North, South, East);
   type Color is (Red, Green, Blue);
   type Flag is (On, Off, Maybe);
   Green_Color : constant Color := Green;
begin
   null;
end Debug_Info_Enum_Shadow;
