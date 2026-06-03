-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- A block nested inside another block: both dissolve, and the scope stack
-- qualifies the innermost subprogram with both block segments. The call, left
-- in the enclosing body, still resolves to it after hoisting.
-- MLIR-LABEL: ada.subp @nested_block
-- MLIR:         ada.decls {
-- MLIR:           ada.subp private @nested_block.outer.inner.deep
-- MLIR:         ada.call @nested_block.outer.inner.deep() : () -> ()
-- MLIR:         ada.return

-- LLVM-LABEL: define void @_ada_nested_block(
-- LLVM:          call void @nested_block__outer__inner__deep()
-- LLVM-LABEL: define internal void @nested_block__outer__inner__deep(

procedure Nested_Block is
begin
   Outer :
   begin
      Inner :
         declare
            procedure Deep is begin null; end Deep;
         begin
            Deep;
         end Inner;
   end Outer;
end Nested_Block;
