-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- Unary `abs` on a signed integer overflows on `Integer'First` (RM 4.5(11),
-- 3.5.4(9)).
-- Modular and floating point operands need no check.

-- CHECK-LABEL: ada.subp private @unary_abs.on_integer(
-- CHECK:         ada.unop "abs" %arg{{[0-9]+}} checks<overflow> : !ada.qual<i32, @standard.integer>

-- CHECK-LABEL: ada.subp private @unary_abs.on_modular(
-- CHECK:         ada.unop "abs" %arg{{[0-9]+}} : !ada.qual<i8, @unary_abs.byte>

-- CHECK-LABEL: ada.subp private @unary_abs.on_float(
-- CHECK:         ada.unop "abs" %arg{{[0-9]+}} : !ada.qual<f32, @standard.float>

procedure Unary_Abs is
   type Byte is mod 256;

   function On_Integer (X : Integer) return Integer is (abs X);
   function On_Modular (X : Byte) return Byte is (abs X);
   function On_Float (X : Float) return Float is (abs X);
begin
   null;
end Unary_Abs;
