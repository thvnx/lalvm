-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM
-- RUN: %lalvm -O1 --emit=llvm --mlir-print-ir-after=ada-lower-references %s -o /dev/null 2>&1 | %FileCheck %s --check-prefix=SSA --implicit-check-not=ada.object --implicit-check-not=memref.alloca --implicit-check-not=memref.load --implicit-check-not=memref.store

-- An in out formal receives a saved value and returns its final value.

-- MLIR-LABEL: ada.subp @param_in_out
-- MLIR:         %[[N_PTR:.*]] = ada.object : !ada.ref<!ada.qual<i32, @standard.integer>>
-- MLIR:         ada.decls {
-- MLIR:           ada.subp private @param_in_out.increment(%x: !ada.ref<!ada.qual<i32, @standard.integer>> {ada.mode = #ada<mode in_out>})
-- MLIR:             %[[V:.*]] = ada.read %x : !ada.ref<!ada.qual<i32, @standard.integer>> -> !ada.qual<i32, @standard.integer>
-- MLIR:             %[[R:.*]] = ada.binop "+" %[[V]], {{.*}} checks<overflow> : (!ada.qual<i32, @standard.integer>, !ada.qual<i32, @standard.integer>) -> !ada.qual<i32, @standard.integer>
-- MLIR:             ada.write %[[R]], %x : !ada.qual<i32, @standard.integer> -> !ada.ref<!ada.qual<i32, @standard.integer>>
-- MLIR:             ada.return
-- MLIR:         ada.call @param_in_out.increment(%[[N_PTR]]) initial_values({{.*}}) {{.*}} : (!ada.ref<!ada.qual<i32, @standard.integer>>) -> ()
-- MLIR:         %[[N:.*]] = ada.read %[[N_PTR]] : !ada.ref<!ada.qual<i32, @standard.integer>> -> !ada.qual<i32, @standard.integer>
-- MLIR:         ada.return %[[N]] : !ada.qual<i32, @standard.integer>

-- LLVM-LABEL: define i32 @_ada_param_in_out(
-- LLVM-LABEL: define internal i32 @param_in_out__increment(i32
-- LLVM:          load i32, ptr
-- LLVM:          call { i32, i1 } @llvm.sadd.with.overflow.i32
-- LLVM:          store i32
-- LLVM:          ret i32

-- SSA-LABEL: func.func @_ada_param_in_out(
-- SSA: call @param_in_out__increment
-- SSA: return
-- SSA-LABEL: func.func private @param_in_out__increment(
-- SSA: ada.binop "+" %arg0,
-- SSA: return

function Param_In_Out return Integer is
   procedure Increment (X : in out Integer);

   procedure Increment (X : in out Integer) is
   begin
      X := X + 1;
   end Increment;
   N : Integer := 41;
begin
   Increment (N);
   return N;
end Param_In_Out;
