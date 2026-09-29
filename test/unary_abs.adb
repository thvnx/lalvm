-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- Unary `abs` on a signed integer overflows on `Integer'First` (RM 4.5(11),
-- 3.5.4(9)). Modular and floating point operands need no check.

-- MLIR-LABEL: ada.subp private @unary_abs.on_integer(
-- MLIR:          ada.unop "abs" %arg{{[0-9]+}} checks<overflow> : !ada.qual<i32, @standard.integer>

-- MLIR-LABEL: ada.subp private @unary_abs.on_modular(
-- MLIR:          ada.unop "abs" %arg{{[0-9]+}} : !ada.qual<i8, @unary_abs.byte>

-- MLIR-LABEL: ada.subp private @unary_abs.on_float(
-- MLIR:          ada.unop "abs" %arg{{[0-9]+}} : !ada.qual<f32, @standard.float>

-- LLVM-LABEL: define internal i32 @unary_abs__on_integer(
-- LLVM-NEXT:     [[ISNEG:%[0-9]+]] = icmp slt i32 %0, 0
-- LLVM-NEXT:     [[WO:%[0-9]+]] = call { i32, i1 } @llvm.ssub.with.overflow.i32(i32 0, i32 %0)
-- LLVM-NEXT:     [[NEG:%[0-9]+]] = extractvalue { i32, i1 } [[WO]], 0
-- LLVM-NEXT:     [[OVF:%[0-9]+]] = extractvalue { i32, i1 } [[WO]], 1
-- LLVM-NEXT:     br i1 [[OVF]], label %[[RAISE:[0-9]+]], label %[[OK:[0-9]+]]
-- LLVM:        [[RAISE]]:
-- LLVM-NEXT:     call void @__gnat_rcheck_CE_Overflow_Check(ptr @lalvm.file, i32 {{[0-9]+}})
-- LLVM-NEXT:     unreachable
-- LLVM:        [[OK]]:
-- LLVM-NEXT:     [[R:%[0-9]+]] = select i1 [[ISNEG]], i32 [[NEG]], i32 %0
-- LLVM-NEXT:     ret i32 [[R]]

-- LLVM-LABEL: define internal i8 @unary_abs__on_modular(
-- LLVM-NEXT:     ret i8 %0

-- LLVM-LABEL: define internal float @unary_abs__on_float(
-- LLVM-NEXT:     [[R:%[0-9]+]] = call float @llvm.fabs.f32(float %0)
-- LLVM-NEXT:     ret float [[R]]

procedure Unary_Abs is
   type Byte is mod 256;

   function On_Integer (X : Integer) return Integer is (abs X);
   function On_Modular (X : Byte) return Byte is (abs X);
   function On_Float (X : Float) return Float is (abs X);
begin
   null;
end Unary_Abs;
