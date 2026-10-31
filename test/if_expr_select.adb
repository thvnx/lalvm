-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm -O1 --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm -O1 --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- At -O1, an if expression yielding values from outside it becomes a select
-- on `!ada.qual` values, which lowers to an LLVM `select`.

-- MLIR: arith.select %{{.*}}, %a, %b : !ada.qual<i32, @standard.integer>

-- LLVM-LABEL: define i32 @_ada_if_expr_select(
-- LLVM-NOT:     br
-- LLVM:         select i1 %{{.*}}, i32 %1, i32 %2

function If_Expr_Select (Cond : Boolean; A, B : Integer) return Integer is
begin
   return (if Cond then A else B);
end If_Expr_Select;
