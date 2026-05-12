-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.proc @test_block
-- MLIR:         ada.block_stmt "Outer" {
-- MLIR:         ada.block_stmt {
-- MLIR:         ada.return

-- LLVM-LABEL: define void @test_block(
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
