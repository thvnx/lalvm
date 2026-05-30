-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- With the alloca model, the Swap block body correctly emits loads and stores
-- that propagate assignments through the enclosing-scope alloca pointers for
-- U and V.
-- MLIR-LABEL: ada.subp @block_swap
-- MLIR:         %[[INIT_U:.*]] = ada.constant : !ada.qual<i32, @standard.integer> = 5
-- MLIR:         %[[U_PTR:.*]] = ada.alloca : memref<!ada.qual<i32, @standard.integer>>
-- MLIR:         memref.store %[[INIT_U]], %[[U_PTR]][] : memref<!ada.qual<i32, @standard.integer>>
-- MLIR:         %[[INIT_V:.*]] = ada.constant : !ada.qual<i32, @standard.integer> = 3
-- MLIR:         %[[V_PTR:.*]] = ada.alloca : memref<!ada.qual<i32, @standard.integer>>
-- MLIR:         memref.store %[[INIT_V]], %[[V_PTR]][] : memref<!ada.qual<i32, @standard.integer>>
-- MLIR:         ada.block "Swap" {
-- MLIR:           %[[V0:.*]] = memref.load %[[V_PTR]][] : memref<!ada.qual<i32, @standard.integer>>
-- MLIR:           %[[U0:.*]] = memref.load %[[U_PTR]][] : memref<!ada.qual<i32, @standard.integer>>
-- MLIR:           memref.store %[[U0]], %[[V_PTR]][] : memref<!ada.qual<i32, @standard.integer>>
-- MLIR:           memref.store %[[V0]], %[[U_PTR]][] : memref<!ada.qual<i32, @standard.integer>>
-- MLIR:         }
-- MLIR:         %[[U1:.*]] = memref.load %[[U_PTR]][] : memref<!ada.qual<i32, @standard.integer>>
-- MLIR:         ada.return %[[U1]] : !ada.qual<i32, @standard.integer>

-- LLVM mem2reg cannot cross the ada.block region boundary, so the
-- alloca for U survives lowering. The function still returns the correct
-- runtime value (original V = 3) through a load of the swapped alloca.
-- LLVM-LABEL: define i32 @_ada_block_swap(
-- LLVM:          ret i32

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
