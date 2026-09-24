-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- An expression function is a body that returns its expression.

-- MLIR-LABEL: ada.subp @expr_function() {
-- MLIR:         ada.decls {
-- MLIR-NEXT:      ada.subp private @expr_function.f() -> !ada.qual<i32, @standard.integer> {
-- MLIR:             %[[C:.*]] = ada.constant : !ada.qual<i32, @standard.integer> = 42
-- MLIR-NEXT:        ada.return %[[C]] : !ada.qual<i32, @standard.integer>
-- MLIR-NEXT:      }

-- LLVM:      define internal i32 @expr_function__f() {
-- LLVM-NEXT:   ret i32 42

procedure Expr_Function is
   function F return Integer is (42);
begin
   null;
end Expr_Function;
