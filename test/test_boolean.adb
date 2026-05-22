-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- Boolean has 2 literals (False=0, True=1) so it maps to i1.
-- Predefined types are lazily emitted at module level on first use.
-- MLIR: ada.type @standard.boolean : i1 = #ada.enum_info<"false" = 0, "true" = 1>
-- MLIR-LABEL: ada.subp @test_boolean
-- MLIR:         %[[C:.*]] = arith.constant {ada.type = @standard.boolean} true
-- MLIR-NEXT:    %[[PTR:.*]] = memref.alloca() {ada.type = @standard.boolean} : memref<i1>
-- MLIR-NEXT:    memref.store %[[C]], %[[PTR]][] : memref<i1>
-- MLIR-NEXT:    ada.null
-- MLIR-NEXT:    ada.return

-- LLVM-LABEL: define void @_ada_test_boolean(
-- LLVM:          ret void

procedure Test_Boolean is
   B : Boolean := True;
begin
   null;
end Test_Boolean;
