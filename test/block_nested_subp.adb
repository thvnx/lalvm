-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- Regression test: a subprogram declared in a block's declarative part must
-- be found by lookupCallee, which walks all SymbolTable scopes including
-- BlockOp.

-- MLIR-LABEL: ada.subp @block_nested_subp
-- MLIR:         ada.block @block_nested_subp.b {
-- MLIR:           ada.subp @block_nested_subp.b.inner
-- MLIR:             ada.return
-- MLIR:           ada.call @block_nested_subp.b.inner() : () -> ()
-- MLIR:         }
-- MLIR:         ada.return

-- LLVM-LABEL: define void @_ada_block_nested_subp(
-- LLVM:          call void @block_nested_subp__b__inner()
-- LLVM-LABEL: define void @block_nested_subp__b__inner(

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
