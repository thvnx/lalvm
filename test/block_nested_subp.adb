-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- Regression test: a subprogram declared in a block's declarative part must
-- be found by lookupCallee, which walks all SymbolTable scopes including
-- BlockOp.

-- MLIR-LABEL: ada.subp @test_block_nested_subp
-- MLIR:         ada.block {
-- MLIR:           ada.subp @inner
-- MLIR:             ada.return
-- MLIR:           ada.call @inner() : () -> ()
-- MLIR:         }
-- MLIR:         ada.return

-- LLVM-LABEL: define void @_ada_test_block_nested_subp(
-- LLVM:          call void @test_block_nested_subp__inner()
-- LLVM-LABEL: define void @test_block_nested_subp__inner(

procedure Test_Block_Nested_Subp is
begin
   declare
      procedure Inner is
      begin
         null;
      end Inner;
   begin
      Inner;
   end;
end Test_Block_Nested_Subp;
