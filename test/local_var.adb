-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.subp @test_local_var
-- MLIR-SAME:    () -> i32
-- MLIR:         %[[X:.*]] = arith.constant 5 : i32
-- MLIR-NEXT:    %[[Y:.*]] = arith.constant 3 : i32
-- MLIR-NEXT:    %[[R:.*]] = ada.binop "+" %[[X]], %[[Y]] : i32
-- MLIR-NEXT:    ada.return %[[R]] : i32

-- LLVM-LABEL: define i32 @_ada_test_local_var(
-- LLVM:         ret i32 8

function Test_Local_Var return Integer is
   X : Integer := 5;
   Y : Integer := 3;
begin
   return X + Y;
end Test_Local_Var;
