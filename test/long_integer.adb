-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.subp @long_integer
-- MLIR-SAME:    (%a: !ada.qual<i64, @standard.long_integer>, %b: !ada.qual<i64, @standard.long_integer>) -> !ada.qual<i64, @standard.long_integer>
-- MLIR:         %[[R:.*]] = ada.binop "+" %a, %b checks<overflow> : (!ada.qual<i64, @standard.long_integer>, !ada.qual<i64, @standard.long_integer>) -> !ada.qual<i64, @standard.long_integer>
-- MLIR-NEXT:    ada.return %[[R]] : !ada.qual<i64, @standard.long_integer>

-- LLVM-LABEL: define i64 @_ada_long_integer(
-- LLVM:         %{{.*}} = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %0, i64 %1)
-- LLVM:         ret i64

function Long_Integer (A, B : Long_Integer) return Long_Integer is
begin
   return A + B;
end Long_Integer;
