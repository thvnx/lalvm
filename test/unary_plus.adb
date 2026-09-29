-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- Unary `+` need no check, it is the identity operation.

-- MLIR-LABEL: ada.subp private @unary_plus.on_integer(
-- MLIR:          ada.unop "+" %arg{{[0-9]+}} : !ada.qual<i32, @standard.integer>

-- MLIR-LABEL: ada.subp private @unary_plus.on_modular(
-- MLIR:          ada.unop "+" %arg{{[0-9]+}} : !ada.qual<i8, @unary_plus.byte>

-- MLIR-LABEL: ada.subp private @unary_plus.on_float(
-- MLIR:          ada.unop "+" %arg{{[0-9]+}} : !ada.qual<f32, @standard.float>

-- LLVM-LABEL: define internal i32 @unary_plus__on_integer(
-- LLVM-NEXT:     ret i32 %0

-- LLVM-LABEL: define internal i8 @unary_plus__on_modular(
-- LLVM-NEXT:     ret i8 %0

-- LLVM-LABEL: define internal float @unary_plus__on_float(
-- LLVM-NEXT:     ret float %0

procedure Unary_Plus is
   type Byte is mod 256;

   function On_Integer (X : Integer) return Integer is (+X);
   function On_Modular (X : Byte) return Byte is (+X);
   function On_Float (X : Float) return Float is (+X);
begin
   null;
end Unary_Plus;
