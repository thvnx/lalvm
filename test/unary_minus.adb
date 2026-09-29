-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- Unary `-` on a signed integer overflows on `Integer'First` (RM 4.5(11),
-- 3.5.4(9)). Modular and floating point operands need no check. On a modulus
-- other than 2**width, `-X` is `m - X` for a nonzero `X` (RM 4.5.4(3)).

-- MLIR-LABEL: ada.subp private @unary_minus.on_integer(
-- MLIR:          ada.unop "-" %arg{{[0-9]+}} checks<overflow> : !ada.qual<i32, @standard.integer>

-- MLIR-LABEL: ada.subp private @unary_minus.on_modular(
-- MLIR:          ada.unop "-" %arg{{[0-9]+}} : !ada.qual<i8, @unary_minus.byte>

-- MLIR-LABEL: ada.subp private @unary_minus.on_mod10(
-- MLIR:          ada.unop "-" %arg{{[0-9]+}} : !ada.qual<i8, @unary_minus.m10>

-- MLIR-LABEL: ada.subp private @unary_minus.on_float(
-- MLIR:          ada.unop "-" %arg{{[0-9]+}} : !ada.qual<f32, @standard.float>

-- LLVM-LABEL: define internal i32 @unary_minus__on_integer(
-- LLVM-NEXT:     [[WO:%[0-9]+]] = call { i32, i1 } @llvm.ssub.with.overflow.i32(i32 0, i32 %0)
-- LLVM-NEXT:     [[NEG:%[0-9]+]] = extractvalue { i32, i1 } [[WO]], 0
-- LLVM-NEXT:     [[OVF:%[0-9]+]] = extractvalue { i32, i1 } [[WO]], 1
-- LLVM-NEXT:     br i1 [[OVF]], label %[[RAISE:[0-9]+]], label %[[OK:[0-9]+]]
-- LLVM:        [[RAISE]]:
-- LLVM-NEXT:     call void @__gnat_rcheck_CE_Overflow_Check(ptr @lalvm.file, i32 {{[0-9]+}})
-- LLVM-NEXT:     unreachable
-- LLVM:        [[OK]]:
-- LLVM-NEXT:     ret i32 [[NEG]]

-- LLVM-LABEL: define internal i8 @unary_minus__on_modular(
-- LLVM-NEXT:     [[R:%[0-9]+]] = sub i8 0, %0
-- LLVM-NEXT:     ret i8 [[R]]

-- LLVM-LABEL: define internal i8 @unary_minus__on_mod10(
-- LLVM-NEXT:     [[Z:%[0-9]+]] = icmp eq i8 %0, 0
-- LLVM-NEXT:     [[S:%[0-9]+]] = sub i8 10, %0
-- LLVM-NEXT:     [[R:%[0-9]+]] = select i1 [[Z]], i8 0, i8 [[S]]
-- LLVM-NEXT:     ret i8 [[R]]

-- LLVM-LABEL: define internal float @unary_minus__on_float(
-- LLVM-NEXT:     [[R:%[0-9]+]] = fneg float %0
-- LLVM-NEXT:     ret float [[R]]

procedure Unary_Minus is
   type Byte is mod 256;
   type M10 is mod 10;

   function On_Integer (X : Integer) return Integer is (-X);
   function On_Modular (X : Byte) return Byte is (-X);
   function On_Mod10 (X : M10) return M10 is (-X);
   function On_Float (X : Float) return Float is (-X);
begin
   null;
end Unary_Minus;
