-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.subp @test_local_var
-- MLIR-SAME:    () -> !ada.qual<i32, @standard.integer>
-- MLIR:         %[[X:.*]] = ada.constant : !ada.qual<i32, @standard.integer> = 5
-- MLIR-NEXT:    %[[Y:.*]] = ada.constant : !ada.qual<i32, @standard.integer> = 3
-- MLIR-NEXT:    %[[R:.*]] = ada.binop "+" %[[X]], %[[Y]] : !ada.qual<i32, @standard.integer>
-- MLIR-NEXT:    ada.return %[[R]] : !ada.qual<i32, @standard.integer>

-- LLVM-LABEL: define i32 @_ada_test_local_var(
-- LLVM:         ret i32 8

function Test_Local_Var return Integer is
   X : Integer := 5;
   Y : Integer := 3;
begin
   return X + Y;
end Test_Local_Var;
