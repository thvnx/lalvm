-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.subp @local_var_init_from_expr
-- MLIR-SAME:    (%arg0: !ada.qual<i32, @standard.integer>, %arg1: !ada.qual<i32, @standard.integer>) -> !ada.qual<i32, @standard.integer>
-- MLIR:         %[[X:.*]] = ada.binop "+" %arg0, %arg1 checks<overflow> : !ada.qual<i32, @standard.integer>
-- MLIR-NEXT:    ada.return %[[X]] : !ada.qual<i32, @standard.integer>

-- LLVM-LABEL: define i32 @_ada_local_var_init_from_expr(
-- LLVM:         add i32
-- LLVM:         ret i32

function Local_Var_Init_From_Expr (A, B : Integer) return Integer is
   X : Integer := A + B;
begin
   return X;
end Local_Var_Init_From_Expr;
