-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.subp @test_block
-- MLIR:         ada.block "Outer" {
-- MLIR:         ada.block {
-- MLIR:         ada.return

-- LLVM-LABEL: define void @_ada_test_block(
-- LLVM:          ret void

procedure Test_Block is
begin
   Outer :
   declare
   begin
      null;
   end Outer;
   begin
      null;
   end;
end Test_Block;
