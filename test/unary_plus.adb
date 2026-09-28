-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- Unary `+` on a signed integer never overflows.
-- Modular and floating point operands need no check.

-- CHECK-LABEL: ada.subp private @unary_plus.on_integer(
-- CHECK:         ada.unop "+" %arg{{[0-9]+}} : !ada.qual<i32, @standard.integer>

-- CHECK-LABEL: ada.subp private @unary_plus.on_modular(
-- CHECK:         ada.unop "+" %arg{{[0-9]+}} : !ada.qual<i8, @unary_plus.byte>

-- CHECK-LABEL: ada.subp private @unary_plus.on_float(
-- CHECK:         ada.unop "+" %arg{{[0-9]+}} : !ada.qual<f32, @standard.float>

procedure Unary_Plus is
   type Byte is mod 256;

   function On_Integer (X : Integer) return Integer is (+X);
   function On_Modular (X : Byte) return Byte is (+X);
   function On_Float (X : Float) return Float is (+X);
begin
   null;
end Unary_Plus;
