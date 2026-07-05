-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- Verify the alloca-based IR at -O0 (the default): locals stay in memory, so
-- B's alloca and store are visible. -O1 runs mem2reg to promote them to SSA,
-- leaving just the constant (see test_boolean.adb).
-- CHECK-LABEL: ada.subp @test_boolean_pre_mem2reg
-- CHECK:         %[[C:.*]] = ada.constant : !ada.qual<i1, @standard.boolean> = true
-- CHECK-NEXT:    %[[PTR:.*]] = ada.alloca : memref<!ada.qual<i1, @standard.boolean>>
-- CHECK-NEXT:    memref.store %[[C]], %[[PTR]][] : memref<!ada.qual<i1, @standard.boolean>>
-- CHECK-NEXT:    ada.null
-- CHECK-NEXT:    ada.return

procedure Test_Boolean_Pre_Mem2reg is
   B : Boolean := True;
begin
   null;
end Test_Boolean_Pre_Mem2reg;
