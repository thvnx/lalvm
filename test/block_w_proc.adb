-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- Each block dissolves into the enclosing subprogram, so its nested subprogram
-- lands in an in-place `ada.decls` with a symbol qualified by the block scope
-- (`outer`, and `b` for the unnamed block).
-- MLIR-LABEL: ada.subp @block_w_proc
-- MLIR:         ada.decls {
-- MLIR:           ada.subp private @block_w_proc.outer.foo
-- MLIR:         ada.decls {
-- MLIR:           ada.subp private @block_w_proc.b.bar
-- MLIR:         ada.return

-- The nested subprograms hoist to module level with GNAT names that fold in
-- the block segment.
-- LLVM-LABEL: define void @_ada_block_w_proc(
-- LLVM-DAG:    define internal void @block_w_proc__outer__foo(
-- LLVM-DAG:    define internal void @block_w_proc__b__bar(

procedure Block_W_Proc is
begin
   Outer :
      declare
         procedure Foo is begin null; end Foo;
      begin
         null;
      end Outer;
   declare
      procedure Bar is begin null; end Bar;
   begin
      null;
   end;
end Block_W_Proc;
