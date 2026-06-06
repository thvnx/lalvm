-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- An if expression (RM 4.5.7) yields a value, so it lowers to scf.if -> (T)
-- with each dependent_expression yielded from its region; SCFToControlFlow
-- then takes it to cf/LLVM (a branch plus a phi at the merge).

-- MLIR-LABEL: ada.subp @if_expr
-- MLIR:         %[[C:.*]] = ada.unwrap %{{.*}} : !ada.qual<i1, @standard.boolean> to i1
-- MLIR:         %[[R:.*]] = scf.if %[[C]] -> (!ada.qual<i32, @standard.integer>) {
-- MLIR:           scf.yield %{{.*}} : !ada.qual<i32, @standard.integer>
-- MLIR:         } else {
-- MLIR:           scf.yield %{{.*}} : !ada.qual<i32, @standard.integer>
-- MLIR:         }
-- MLIR:         ada.return %[[R]] : !ada.qual<i32, @standard.integer>

-- LLVM-LABEL: define i32 @_ada_if_expr(
-- LLVM:         br i1
-- LLVM:         phi i32
-- LLVM:         ret i32

function If_Expr (Cond : Boolean; A, B : Integer) return Integer is
begin
   return (if Cond then A else B);
end If_Expr;
