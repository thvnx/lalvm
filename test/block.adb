-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.subp @block
-- MLIR:         ada.block @block.outer {
-- MLIR:         ada.block @block.b {
-- MLIR:         ada.return

-- LLVM-LABEL: define void @_ada_block(
-- LLVM:          ret void

procedure Block is
begin
   Outer :
   declare
   begin
      null;
   end Outer;
   begin
      null;
   end;
end Block;
