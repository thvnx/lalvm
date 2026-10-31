-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.subp @short_integer_mul
-- MLIR-SAME:    (%a: !ada.qual<i16, @standard.short_integer>, %b: !ada.qual<i16, @standard.short_integer>) -> !ada.qual<i16, @standard.short_integer>
-- MLIR:         %[[R:.*]] = ada.binop "*" %a, %b checks<overflow> : (!ada.qual<i16, @standard.short_integer>, !ada.qual<i16, @standard.short_integer>) -> !ada.qual<i16, @standard.short_integer>
-- MLIR-NEXT:    ada.return %[[R]] : !ada.qual<i16, @standard.short_integer>

-- LLVM-LABEL: define i16 @_ada_short_integer_mul(
-- LLVM:         %{{.*}} = call { i16, i1 } @llvm.smul.with.overflow.i16(i16 %0, i16 %1)
-- LLVM:         ret i16

function Short_Integer_Mul (A, B : Short_Integer) return Short_Integer is
begin
   return A * B;
end Short_Integer_Mul;
