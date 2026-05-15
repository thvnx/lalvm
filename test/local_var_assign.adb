-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.func @test_local_var_assign
-- MLIR-SAME:    () -> i32
-- MLIR:         %[[INIT:.*]] = arith.constant 4 : i32
-- MLIR-NEXT:    %[[ONE:.*]] = arith.constant 1 : i32
-- MLIR-NEXT:    %[[X:.*]] = ada.binop "+" %[[INIT]], %[[ONE]] : i32
-- MLIR-NEXT:    ada.return %[[X]] : i32

-- LLVM-LABEL: define i32 @_ada_test_local_var_assign(
-- LLVM:         ret i32 5

function Test_Local_Var_Assign return Integer is
   X : Integer := 4;
begin
   X := X + 1;
   return X;
end Test_Local_Var_Assign;
