-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.subp @test_local_var_assign
-- MLIR-SAME:    () -> !ada.qual<i32, @standard.integer>
-- MLIR:         %[[INIT:.*]] = ada.constant : !ada.qual<i32, @standard.integer> = 4
-- MLIR-NEXT:    %[[ONE:.*]] = ada.constant : !ada.qual<i32, @standard.integer> = 1
-- MLIR-NEXT:    %[[X:.*]] = ada.binop "+" %[[INIT]], %[[ONE]] : !ada.qual<i32, @standard.integer>
-- MLIR-NEXT:    ada.return %[[X]] : !ada.qual<i32, @standard.integer>

-- LLVM-LABEL: define i32 @_ada_test_local_var_assign(
-- LLVM:         ret i32 5

function Test_Local_Var_Assign return Integer is
   X : Integer := 4;
begin
   X := X + 1;
   return X;
end Test_Local_Var_Assign;
