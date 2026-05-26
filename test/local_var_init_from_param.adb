-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.subp @test_local_var_init_from_param
-- MLIR-SAME:    (%arg0: !ada.qual<i32, @standard.integer>) -> !ada.qual<i32, @standard.integer>
-- MLIR:         ada.return %arg0 : !ada.qual<i32, @standard.integer>

-- LLVM-LABEL: define i32 @_ada_test_local_var_init_from_param(
-- LLVM:         ret i32 %0

function Test_Local_Var_Init_From_Param (N : Integer) return Integer is
   X : Integer := N;
begin
   return X;
end Test_Local_Var_Init_From_Param;
