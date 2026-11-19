-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm -O0 --emit=llvm %s | %FileCheck %s --check-prefix=LLVM
-- RUN: %lalvm -O1 --emit=mlir %s | %FileCheck %s --check-prefix=MLIR-OPT
-- RUN: %lalvm -O1 --emit=llvm %s | %FileCheck %s --check-prefix=LLVM-OPT

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

-- At -O1, an if expression yielding values from outside it becomes a select
-- on `!ada.qual` values, which lowers to an LLVM `select`.

-- MLIR-OPT: arith.select %{{.*}}, %a, %b : !ada.qual<i32, @standard.integer>

-- LLVM-OPT-LABEL: define i32 @_ada_if_expr(
-- LLVM-OPT-NOT:     br
-- LLVM-OPT:         select i1 %{{.*}}, i32 %1, i32 %2

function If_Expr (Cond : Boolean; A, B : Integer) return Integer is
begin
   return (if Cond then A else B);
end If_Expr;
