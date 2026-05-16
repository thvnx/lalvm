-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR

-- 2 literals -> i1; On has position 1.
-- MLIR-LABEL: ada.subp @test_enum_type_i1
-- MLIR:         %true = arith.constant true
-- MLIR-NEXT:    ada.null
-- MLIR-NEXT:    ada.return

procedure Test_Enum_Type_I1 is
   type Switch is (Off, On);
   D : constant Switch := On;
begin
   null;
end Test_Enum_Type_I1;
