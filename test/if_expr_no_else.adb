-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- With no else, the if expression is of a boolean type and the absent else
-- yields True (RM 4.5.7(18/3, 20/3)). The implicit True is a synthesized
-- ada.constant of the Boolean type.

-- MLIR-LABEL: ada.subp @if_expr_no_else
-- MLIR:         %[[C:.*]] = ada.unwrap %{{.*}} : !ada.qual<i1, @standard.boolean> to i1
-- MLIR:         %[[R:.*]] = scf.if %[[C]] -> (!ada.qual<i1, @standard.boolean>) {
-- MLIR:           scf.yield %{{.*}} : !ada.qual<i1, @standard.boolean>
-- MLIR:         } else {
-- MLIR:           %[[T:.*]] = ada.constant : !ada.qual<i1, @standard.boolean> = true
-- MLIR:           scf.yield %[[T]] : !ada.qual<i1, @standard.boolean>
-- MLIR:         }
-- MLIR:         ada.return %[[R]] : !ada.qual<i1, @standard.boolean>

-- LLVM-LABEL: define i1 @_ada_if_expr_no_else(
-- LLVM:         br i1
-- LLVM:         ret i1

function If_Expr_No_Else (P, Q : Boolean) return Boolean is
begin
   return (if P then Q);
end If_Expr_No_Else;
