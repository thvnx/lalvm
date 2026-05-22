-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.subp @test_local_var_assign
-- MLIR-SAME:    () -> i32
-- MLIR:         %[[INIT:.*]] = arith.constant {ada.type = @standard.integer} 4 : i32
-- MLIR-NEXT:    %[[PTR:.*]] = memref.alloca() {ada.type = @standard.integer} : memref<i32>
-- MLIR-NEXT:    memref.store %[[INIT]], %[[PTR]][] : memref<i32>
-- MLIR:         %[[X0:.*]] = memref.load %[[PTR]][] : memref<i32>
-- MLIR:         %[[ONE:.*]] = arith.constant {ada.type = @standard.integer} 1 : i32
-- MLIR:         %[[X1:.*]] = ada.binop "+" %[[X0]], %[[ONE]] {ada.type = @standard.integer} : i32
-- MLIR:         memref.store %[[X1]], %[[PTR]][] : memref<i32>
-- MLIR:         %[[X2:.*]] = memref.load %[[PTR]][] : memref<i32>
-- MLIR:         ada.return %[[X2]] : i32

-- LLVM-LABEL: define i32 @_ada_test_local_var_assign(
-- LLVM:         ret i32 5

function Test_Local_Var_Assign return Integer is
   X : Integer := 4;
begin
   X := X + 1;
   return X;
end Test_Local_Var_Assign;
