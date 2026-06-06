-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- An elsif part nests as an scf.if in the else region, so conditions are
-- tested in order and the first True wins (RM 4.5.7(20/3)).

-- MLIR-LABEL: ada.subp @if_expr_elsif
-- MLIR:         %[[CP:.*]] = ada.unwrap %{{.*}} : !ada.qual<i1, @standard.boolean> to i1
-- MLIR:         %[[OUT:.*]] = scf.if %[[CP]] -> (!ada.qual<i32, @standard.integer>) {
-- MLIR:           scf.yield %{{.*}} : !ada.qual<i32, @standard.integer>
-- MLIR:         } else {
-- MLIR:           %[[CQ:.*]] = ada.unwrap %{{.*}} : !ada.qual<i1, @standard.boolean> to i1
-- MLIR:           %[[IN:.*]] = scf.if %[[CQ]] -> (!ada.qual<i32, @standard.integer>) {
-- MLIR:             scf.yield %{{.*}} : !ada.qual<i32, @standard.integer>
-- MLIR:           } else {
-- MLIR:             scf.yield %{{.*}} : !ada.qual<i32, @standard.integer>
-- MLIR:           }
-- MLIR:           scf.yield %[[IN]] : !ada.qual<i32, @standard.integer>
-- MLIR:         }
-- MLIR:         ada.return %[[OUT]] : !ada.qual<i32, @standard.integer>

-- LLVM-LABEL: define i32 @_ada_if_expr_elsif(
-- LLVM:         br i1
-- LLVM:         br i1
-- LLVM:         ret i32

function If_Expr_Elsif (P, Q : Boolean; A, B, C : Integer) return Integer is
begin
   return (if P then A elsif Q then B else C);
end If_Expr_Elsif;
