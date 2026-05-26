-- RUN: %lalvm --emit=mlir --mlir-print-ir-before=mem2reg %s 2>&1 1>/dev/null | %FileCheck %s

-- Verify the alloca-based IR that mem2reg promotes. --mlir-print-ir-before=mem2reg
-- captures the module before SSA promotion; B's alloca and store are visible here
-- but absent from the normal --emit=mlir output (which is post-mem2reg).
-- CHECK-LABEL: ada.subp @test_boolean
-- CHECK:         %[[C:.*]] = arith.constant true
-- CHECK-NEXT:    %[[PTR:.*]] = memref.alloca() : memref<i1>
-- CHECK-NEXT:    memref.store %[[C]], %[[PTR]][] : memref<i1>
-- CHECK-NEXT:    ada.null
-- CHECK-NEXT:    ada.return

procedure Test_Boolean is
   B : Boolean := True;
begin
   null;
end Test_Boolean;
