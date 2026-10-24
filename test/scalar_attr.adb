-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- `'First`/`'Last` (RM 3.5), per bound. `range 1 .. N` has a static lower bound
-- and a dynamic upper bound, so `S'First` folds to the constant 1 (read from
-- the subtype's `int_info`) while `S'Last` is read at run time from the range
-- descriptor elaborated at the subtype declaration, via `ada.attr` (lowered to
-- an `extractvalue` of field 1).
-- MLIR-LABEL: ada.subp @scalar_attr
-- MLIR:         %[[R:.*]] = ada.range %{{.*}}, %{{.*}} : !ada.qual<i32, @standard.integer> -> !ada.range<i32, @scalar_attr.s>
-- MLIR:         %[[L:.*]] = ada.attr "last", %[[R]] : !ada.qual<i32, @scalar_attr.s>
-- MLIR:         ada.coerce %[[L]] : <i32, @scalar_attr.s> to <i32, @standard.integer>
-- MLIR:         ada.constant : !ada.qual<i32, @standard.integer> = 1

-- `S'Last - S'First` keeps its overflow check.
-- MLIR:         ada.binop "-" %{{.*}}, %{{.*}} checks<overflow> : (!ada.qual<i32, @standard.integer>, !ada.qual<i32, @standard.integer>) -> !ada.qual<i32, @standard.integer>

-- LLVM-LABEL: define i32 @_ada_scalar_attr(
-- LLVM:         extractvalue { i32, i32 } %{{.*}}, 1

function Scalar_Attr (N : Integer) return Integer is
   subtype S is Integer range 1 .. N;
begin
   return S'Last - S'First;
end Scalar_Attr;
