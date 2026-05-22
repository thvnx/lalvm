-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=DWARF

-- 3 literals -> i8; Green has position 1.
-- MLIR-LABEL: ada.subp @test_enum_type_i8
-- MLIR:         ada.type @color : i8 = #ada.enum_info<"red" = 0, "green" = 1, "blue" = 2>
-- MLIR-NEXT:    %[[C:.*]] = arith.constant{{.*}}1 : i8
-- MLIR-NEXT:    %[[PTR:.*]] = memref.alloca(){{.*}}: memref<i8>
-- MLIR-NEXT:    memref.store %[[C]], %[[PTR]][] : memref<i8>
-- MLIR-NEXT:    ada.null
-- MLIR-NEXT:    ada.return

-- DWARF: !DICompositeType(tag: DW_TAG_enumeration_type, name: "color"
-- DWARF: !DIEnumerator(name: "red", value: 0)
-- DWARF: !DIEnumerator(name: "green", value: 1)
-- DWARF: !DIEnumerator(name: "blue", value: 2)

procedure Test_Enum_Type_I8 is
   type Color is (Red, Green, Blue);
   C : constant Color := Green;
begin
   null;
end Test_Enum_Type_I8;
