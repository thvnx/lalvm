-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- Verify the object and its initializer before reference lowering.
-- CHECK-LABEL: ada.subp @test_boolean_pre_mem2reg
-- CHECK:         %[[C:.*]] = ada.constant : !ada.qual<i1, @standard.boolean> = true
-- CHECK-NEXT:    %[[PTR:.*]] = ada.object : !ada.ref<!ada.qual<i1, @standard.boolean>> <- %[[C]]
-- CHECK-NEXT:    ada.null
-- CHECK-NEXT:    ada.return

procedure Test_Boolean_Pre_Mem2reg is
   B : Boolean := True;
begin
   null;
end Test_Boolean_Pre_Mem2reg;
