-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm -O1 --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm -O1 --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.subp @local_var_assign
-- MLIR-SAME:    () -> !ada.qual<i32, @standard.integer>
-- MLIR-DAG:     %[[INIT:.*]] = ada.constant : !ada.qual<i32, @standard.integer> = 4
-- MLIR-DAG:     %[[ONE:.*]] = ada.constant : !ada.qual<i32, @standard.integer> = 1
-- MLIR:         %[[X:.*]] = ada.binop "+" %[[INIT]], %[[ONE]] checks<overflow> : !ada.qual<i32, @standard.integer>
-- MLIR-NEXT:    ada.return %[[X]] : !ada.qual<i32, @standard.integer>

-- LLVM-LABEL: define i32 @_ada_local_var_assign(
-- LLVM:         ret i32 5

function Local_Var_Assign return Integer is
   X : Integer := 4;
begin
   X := X + 1;
   return X;
end Local_Var_Assign;
