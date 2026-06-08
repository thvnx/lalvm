-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=DWARF

-- 3 literals, max rep 2 -> i3: the smallest width holding the reps as
-- non-negative signed values. The sign bit stays clear, so the max rep
-- prints as 2 (in an i2 it would render as -2).
-- MLIR-LABEL: ada.subp @enum_type_i3
-- MLIR:         %[[C:.*]] = ada.constant : !ada.qual<i3, @enum_type_i3.color> = 1
-- MLIR-NEXT:    %[[C2:.*]] = ada.constant : !ada.qual<i3, @enum_type_i3.color> = 2
-- MLIR-NEXT:    ada.decls {
-- MLIR-NEXT:      ada.type @enum_type_i3.color : i3 = #ada.enum_info<"red" = 0, "green" = 1, "blue" = 2>
-- MLIR:         ada.null
-- MLIR-NEXT:    ada.return

-- DWARF: !DICompositeType(tag: DW_TAG_enumeration_type, name: "color"
-- DWARF: !DIEnumerator(name: "red", value: 0)
-- DWARF: !DIEnumerator(name: "green", value: 1)
-- DWARF: !DIEnumerator(name: "blue", value: 2)

procedure Enum_Type_I3 is
   type Color is (Red, Green, Blue);
   C  : constant Color := Green;
   C2 : constant Color := Blue;
begin
   null;
end Enum_Type_I3;
