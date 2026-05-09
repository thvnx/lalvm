-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.func @test_local_var_init_from_param
-- MLIR-SAME:    (%arg0: i32) -> i32
-- MLIR:         ada.return %arg0 : i32

-- LLVM-LABEL: define i32 @test_local_var_init_from_param(
-- LLVM:         ret i32 %0

function Test_Local_Var_Init_From_Param (N : Integer) return Integer is
   X : Integer := N;
begin
   return X;
end Test_Local_Var_Init_From_Param;
