-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.subp @test_local_var_init_from_expr
-- MLIR-SAME:    (%arg0: i32 {ada.type = @standard.integer}, %arg1: i32 {ada.type = @standard.integer}) -> i32
-- MLIR:         %[[INIT:.*]] = ada.binop "+" %arg0, %arg1 : i32
-- MLIR-NEXT:    %[[PTR:.*]] = memref.alloca(){{.*}}: memref<i32>
-- MLIR-NEXT:    memref.store %[[INIT]], %[[PTR]][] : memref<i32>
-- MLIR:         %[[X:.*]] = memref.load %[[PTR]][] : memref<i32>
-- MLIR:         ada.return %[[X]] : i32

-- LLVM-LABEL: define i32 @_ada_test_local_var_init_from_expr(
-- LLVM:         add i32
-- LLVM:         ret i32

function Test_Local_Var_Init_From_Expr (A, B : Integer) return Integer is
   X : Integer := A + B;
begin
   return X;
end Test_Local_Var_Init_From_Expr;
