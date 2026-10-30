-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- Writable formals are independent objects, returned in formal order.

-- MLIR-LABEL: ada.subp @proc(
-- MLIR-SAME:    %i: !ada.ref<!ada.qual<i32, @standard.integer>> {ada.mode = #ada<mode in_out>}, %j: !ada.ref<!ada.qual<i32, @standard.integer>> {ada.mode = #ada<mode in_out>}, %k: !ada.ref<!ada.qual<i32, @standard.integer>> {ada.mode = #ada<mode in_out>}
-- MLIR:         %[[J:.*]] = ada.read %j : !ada.ref<!ada.qual<i32, @standard.integer>> -> !ada.qual<i32, @standard.integer>
-- MLIR:         %[[K:.*]] = ada.read %k : !ada.ref<!ada.qual<i32, @standard.integer>> -> !ada.qual<i32, @standard.integer>
-- MLIR:         %[[R:.*]] = ada.binop "+" %[[J]], %[[K]] checks<overflow> : (!ada.qual<i32, @standard.integer>, !ada.qual<i32, @standard.integer>) -> !ada.qual<i32, @standard.integer>
-- MLIR:         ada.write %[[R]], %i : !ada.qual<i32, @standard.integer> -> !ada.ref<!ada.qual<i32, @standard.integer>>
-- MLIR:         ada.return

-- LLVM-LABEL: define { i32, i32, i32 } @_ada_proc(i32
-- LLVM:         load i32, ptr
-- LLVM:         load i32, ptr
-- LLVM:         %{{.*}} = call { i32, i1 } @llvm.sadd.with.overflow.i32
-- LLVM:         store i32 %{{.*}}, ptr
-- LLVM:         ret { i32, i32, i32 }

procedure Proc (I, J, K : in out Integer) is
begin
   I := J + K;
end Proc;
