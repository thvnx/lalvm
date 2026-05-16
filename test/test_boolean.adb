-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- Boolean has 2 literals (False=0, True=1) so it maps to i1.
-- MLIR-LABEL: ada.subp @test_boolean
-- MLIR:         %true = arith.constant true
-- MLIR-NEXT:    ada.null
-- MLIR-NEXT:    ada.return

-- LLVM-LABEL: define void @_ada_test_boolean(
-- LLVM:          ret void

procedure Test_Boolean is
   B : Boolean := True;
begin
   null;
end Test_Boolean;
