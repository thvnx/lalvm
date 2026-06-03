-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- XFAIL: *
-- The nested subprogram lives in the block's `ada.block` (a SymbolTable). After
-- `HoistNestedSymbolOperations` lifts it to module level, the call left inside
-- the block no longer resolves to it (a flat ref binds to the block's own
-- table), so `replaceAllSymbolUses` cannot rewrite the call site. Once blocks
-- dissolve into the enclosing CFG, the nested subprogram moves to an `ada.decls`
-- and the call sits in the parent body, resolving upward to the module like
-- every other hoisted call; the checks below already expect that form, so this
-- test will XPASS then and the `XFAIL` can be removed.

-- Regression test: a subprogram declared in a block's declarative part must
-- be found by lookupCallee.

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
