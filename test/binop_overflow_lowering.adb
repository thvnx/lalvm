-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM --implicit-check-not="with.overflow.i8"

-- Signed integer arithmetic checks for overflow (RM 4.5).
-- Modular addition wraps and floating point multiplication is unchecked.
-- MLIR:      ada.binop "+" %{{.*}}, %{{.*}} : (!ada.qual<i8, @binop_overflow_lowering.byte>, !ada.qual<i8, @binop_overflow_lowering.byte>) -> !ada.qual<i8, @binop_overflow_lowering.byte>
-- MLIR:      ada.binop "*" %{{.*}}, %{{.*}} : (!ada.qual<f32, @standard.float>, !ada.qual<f32, @standard.float>) -> !ada.qual<f32, @standard.float>
-- MLIR:      ada.binop "+" %{{.*}}, %{{.*}} checks<overflow> : (!ada.qual<i32, @standard.integer>, !ada.qual<i32, @standard.integer>) -> !ada.qual<i32, @standard.integer>

-- LLVM-LABEL: define i32 @_ada_binop_overflow_lowering(
-- LLVM:         %[[WO:.*]] = call { i32, i1 } @llvm.sadd.with.overflow.i32(i32 %0, i32 %1)
-- LLVM:         %{{.*}} = extractvalue { i32, i1 } %[[WO]], 0
-- LLVM:         %[[O:.*]] = extractvalue { i32, i1 } %[[WO]], 1
-- LLVM:         br i1 %[[O]], label %[[RAISE:[0-9]+]], label %{{[0-9]+}}
-- LLVM:       [[RAISE]]:
-- LLVM:         call void @__lalvm_rcheck_CE_Overflow_Check(ptr {{.*}}, i32 {{.*}})
-- LLVM:         unreachable

-- LLVM-DAG:     call { i32, i1 } @llvm.ssub.with.overflow.i32
-- LLVM-DAG:     call { i32, i1 } @llvm.smul.with.overflow.i32
-- LLVM-DAG:     = add i8
-- LLVM-DAG:     = fmul float

function Binop_Overflow_Lowering (A, B : Integer) return Integer is
   type Byte is mod 256;
   function Diff (X, Y : Integer) return Integer is
   begin
      return X - Y;
   end Diff;
   function Prod (X, Y : Integer) return Integer is
   begin
      return X * Y;
   end Prod;
   function Wrap (X, Y : Byte) return Byte is
   begin
      return X + Y;
   end Wrap;
   function Scale (F, G : Float) return Float is
   begin
      return F * G;
   end Scale;
begin
   return A + B;
end Binop_Overflow_Lowering;
