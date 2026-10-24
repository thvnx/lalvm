-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM
-- RUN: %lalvm --emit=llvm --mlir-print-ir-after=ada-lower-conversions %s -o /dev/null 2>&1 | %FileCheck %s --check-prefix=LOWERED

-- A value flowing into a dynamic-bound subtype is checked against the range
-- elaborated once at the subtype declaration (RM 3.2.2) and reused here.
-- MLIR-LABEL: ada.subp @range_check_dynamic
-- MLIR:         %[[LO:.*]] = ada.constant : !ada.qual<i32, @standard.integer> = 1
-- The dynamic upper bound `N` is of the base type, so it feeds the range directly.
-- MLIR:         %[[R:.*]] = ada.range %[[LO]], %arg0 : !ada.qual<i32, @standard.integer> -> !ada.range<i32, @range_check_dynamic.s>
-- MLIR:         ada.convert implicit %arg1, %[[R]] : !ada.qual<i32, @standard.integer> to !ada.qual<i32, @range_check_dynamic.s>

-- LOWERED-LABEL: ada.subp @range_check_dynamic
-- LOWERED:         %[[R:.*]] = ada.range %{{.*}}, %arg0
-- LOWERED:         ada.range_check %arg1, %[[R]] : !ada.qual<i32, @standard.integer> to !ada.qual<i32, @range_check_dynamic.s>

-- LLVM-LABEL: define i32 @_ada_range_check_dynamic(
-- LLVM:         icmp slt
-- LLVM:         icmp sgt
-- LLVM:         call void @__gnat_rcheck_CE_Range_Check(

function Range_Check_Dynamic (N, M : Integer) return Integer is
   subtype S is Integer range 1 .. N;
   X : S := M;
begin
   return X;
end Range_Check_Dynamic;
