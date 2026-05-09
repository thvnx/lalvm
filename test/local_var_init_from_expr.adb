-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.func @test_local_var_init_from_expr
-- MLIR-SAME:    (%arg0: i32, %arg1: i32) -> i32
-- MLIR:         %[[X:.*]] = ada.add %arg0, %arg1 : i32
-- MLIR-NEXT:    ada.return %[[X]] : i32

-- LLVM-LABEL: define i32 @test_local_var_init_from_expr(
-- LLVM:         add i32
-- LLVM:         ret i32

function Test_Local_Var_Init_From_Expr (A, B : Integer) return Integer is
   X : Integer := A + B;
begin
   return X;
end Test_Local_Var_Init_From_Expr;
