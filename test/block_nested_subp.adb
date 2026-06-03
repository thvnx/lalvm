-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- Regression test: a subprogram declared in a block's declarative part is
-- emitted into the block's in-place `ada.decls` and the call, left in the
-- enclosing body once the block dissolves, resolves to it after hoisting.

-- MLIR-LABEL: ada.subp @block_nested_subp
-- MLIR:         ada.decls {
-- MLIR:           ada.subp private @block_nested_subp.b.inner
-- MLIR:             ada.return
-- MLIR:         ada.call @block_nested_subp.b.inner() : () -> ()
-- MLIR:         ada.return

-- LLVM-LABEL: define void @_ada_block_nested_subp(
-- LLVM:          call void @block_nested_subp__b__inner()
-- LLVM-LABEL: define internal void @block_nested_subp__b__inner(

procedure Block_Nested_Subp is
begin
   declare
      procedure Inner is
      begin
         null;
      end Inner;
   begin
      Inner;
   end;
end Block_Nested_Subp;
