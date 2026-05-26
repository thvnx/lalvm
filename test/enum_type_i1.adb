-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=DWARF

-- 2 literals -> i1; On has position 1.
-- MLIR-LABEL: ada.subp @test_enum_type_i1
-- MLIR:         ada.type @switch : i1 = #ada.enum_info<"off" = 0, "on" = 1>
-- MLIR-NEXT:    %[[C:.*]] = ada.constant : !ada.qual<i1, @switch> = true
-- MLIR-NEXT:    ada.null
-- MLIR-NEXT:    ada.return

-- DWARF: !DICompositeType(tag: DW_TAG_enumeration_type, name: "switch"
-- DWARF: !DIEnumerator(name: "off", value: 0)
-- DWARF: !DIEnumerator(name: "on", value: 1)

procedure Test_Enum_Type_I1 is
   type Switch is (Off, On);
   D : constant Switch := On;
begin
   null;
end Test_Enum_Type_I1;
