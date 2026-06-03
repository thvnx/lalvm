-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- Blocks dissolve into the enclosing subprogram: each block's statements are
-- emitted inline with no `ada.block` op, so the two empty blocks leave just
-- their `null` statements.
-- MLIR-LABEL: ada.subp @block
-- MLIR:         ada.null
-- MLIR-NEXT:    ada.null
-- MLIR-NEXT:    ada.return

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
