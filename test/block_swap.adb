-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- The Swap block dissolves into the enclosing subprogram, so `mem2reg` is no
-- longer blocked by a region boundary: it promotes U, V, and Temp to SSA and
-- the swap folds to value forwarding. The function returns V's initial value
-- (3) with no alloca surviving.
-- MLIR-LABEL: ada.subp @block_swap
-- MLIR:         %[[V:.*]] = ada.constant : !ada.qual<i32, @standard.integer> = 3
-- MLIR:         ada.return %[[V]] : !ada.qual<i32, @standard.integer>

-- LLVM-LABEL: define i32 @_ada_block_swap(
-- LLVM:          ret i32 3

function Block_Swap return Integer is
   U : Integer := 5;
   V : Integer := 3;
begin
   Swap:
      declare
         Temp : Integer;
      begin
         Temp := V; V := U; U := Temp;
      end Swap;
   return U;
end Block_Swap;
