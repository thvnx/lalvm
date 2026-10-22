-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- If expression branches.

-- CHECK: %[[Y:.*]] = ada.coerce %{{.*}} : <i32, @if_expr_join.small> to <i32, @standard.integer>
-- CHECK: scf.yield %[[Y]]

function If_Expr_Join (C : Boolean; I : Integer) return Integer is
   subtype Small is Integer range 1 .. 10;
   Y : Small := 5;
begin
   return (if C then Y else I);
end If_Expr_Join;
