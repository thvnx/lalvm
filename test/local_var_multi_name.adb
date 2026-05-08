-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.func @test_local_var_multi_name
-- MLIR-SAME:    () -> i32
-- MLIR:         %[[V:.*]] = arith.constant 3 : i32
-- MLIR-NEXT:    %[[R1:.*]] = ada.add %[[V]], %[[V]] : i32
-- MLIR-NEXT:    %[[R2:.*]] = ada.add %[[R1]], %[[V]] : i32
-- MLIR-NEXT:    ada.return %[[R2]] : i32

-- LLVM-LABEL: define i32 @test_local_var_multi_name(
-- LLVM:         ret i32 9

function Test_Local_Var_Multi_Name return Integer is
   X, Y, Z : Integer := 3;
begin
   return X + Y + Z;
end Test_Local_Var_Multi_Name;
