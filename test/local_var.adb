-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.subp @test_local_var
-- MLIR-SAME:    () -> i32
-- MLIR:         %[[X_INIT:.*]] = arith.constant {ada.type = @standard.integer} 5 : i32
-- MLIR-NEXT:    %[[X_PTR:.*]] = memref.alloca() {ada.type = @standard.integer} : memref<i32>
-- MLIR-NEXT:    memref.store %[[X_INIT]], %[[X_PTR]][] : memref<i32>
-- MLIR-NEXT:    %[[Y_INIT:.*]] = arith.constant {ada.type = @standard.integer} 3 : i32
-- MLIR-NEXT:    %[[Y_PTR:.*]] = memref.alloca() {ada.type = @standard.integer} : memref<i32>
-- MLIR-NEXT:    memref.store %[[Y_INIT]], %[[Y_PTR]][] : memref<i32>
-- MLIR:         %[[X:.*]] = memref.load %[[X_PTR]][] : memref<i32>
-- MLIR:         %[[Y:.*]] = memref.load %[[Y_PTR]][] : memref<i32>
-- MLIR:         %[[R:.*]] = ada.binop "+" %[[X]], %[[Y]] {ada.type = @standard.integer} : i32
-- MLIR:         ada.return %[[R]] : i32

-- LLVM-LABEL: define i32 @_ada_test_local_var(
-- LLVM:         ret i32 8

function Test_Local_Var return Integer is
   X : Integer := 5;
   Y : Integer := 3;
begin
   return X + Y;
end Test_Local_Var;
