-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- With the alloca model, the Swap block body correctly emits loads and stores
-- that propagate assignments through the enclosing-scope alloca pointers for
-- U and V.
-- MLIR-LABEL: ada.subp @test
-- MLIR:         %[[INIT_U:.*]] = arith.constant {ada.type = @standard.integer} 5 : i32
-- MLIR:         %[[U_PTR:.*]] = memref.alloca() {ada.type = @standard.integer} : memref<i32>
-- MLIR:         memref.store %[[INIT_U]], %[[U_PTR]][] : memref<i32>
-- MLIR:         %[[INIT_V:.*]] = arith.constant {ada.type = @standard.integer} 3 : i32
-- MLIR:         %[[V_PTR:.*]] = memref.alloca() {ada.type = @standard.integer} : memref<i32>
-- MLIR:         memref.store %[[INIT_V]], %[[V_PTR]][] : memref<i32>
-- MLIR:         ada.block_stmt "Swap" {
-- MLIR:           %[[T_PTR:.*]] = memref.alloca() {ada.type = @standard.integer} : memref<i32>
-- MLIR:           %[[V0:.*]] = memref.load %[[V_PTR]][] : memref<i32>
-- MLIR:           memref.store %[[V0]], %[[T_PTR]][] : memref<i32>
-- MLIR:           %[[U0:.*]] = memref.load %[[U_PTR]][] : memref<i32>
-- MLIR:           memref.store %[[U0]], %[[V_PTR]][] : memref<i32>
-- MLIR:           %[[T0:.*]] = memref.load %[[T_PTR]][] : memref<i32>
-- MLIR:           memref.store %[[T0]], %[[U_PTR]][] : memref<i32>
-- MLIR:         }
-- MLIR:         %[[U1:.*]] = memref.load %[[U_PTR]][] : memref<i32>
-- MLIR:         ada.return %[[U1]] : i32

-- LLVM mem2reg cannot cross the ada.block_stmt region boundary, so the
-- alloca for U survives lowering. The function still returns the correct
-- runtime value (original V = 3) through a load of the swapped alloca.
-- LLVM-LABEL: define i32 @_ada_test(
-- LLVM:          ret i32

function Test return Integer is
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
end Test;
