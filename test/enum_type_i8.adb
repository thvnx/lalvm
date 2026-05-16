-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR

-- 3 literals -> i8; Green has position 1.
-- MLIR-LABEL: ada.subp @test_enum_type_i8
-- MLIR:         %[[C:.*]] = arith.constant 1 : i8
-- MLIR-NEXT:    ada.null
-- MLIR-NEXT:    ada.return

procedure Test_Enum_Type_I8 is
   type Color is (Red, Green, Blue);
   C : constant Color := Green;
begin
   null;
end Test_Enum_Type_I8;
