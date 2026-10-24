-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s
-- RUN: %lalvm --emit=llvm --mlir-print-ir-after=ada-lower-conversions %s -o /dev/null 2>&1 | %FileCheck %s --check-prefix=LOWERED

-- If expression branches.

-- CHECK: %[[Y:.*]] = ada.convert implicit %{{.*}} : !ada.qual<i32, @if_expr_join.small> to !ada.qual<i32, @standard.integer>
-- CHECK: scf.yield %[[Y]]

-- LOWERED: %[[Y:.*]] = ada.requalify %{{.*}} : !ada.qual<i32, @if_expr_join.small> to !ada.qual<i32, @standard.integer>
-- LOWERED: scf.yield %[[Y]]

function If_Expr_Join (C : Boolean; I : Integer) return Integer is
   subtype Small is Integer range 1 .. 10;
   Y : Small := 5;
begin
   return (if C then Y else I);
end If_Expr_Join;
