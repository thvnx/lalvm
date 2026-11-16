-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM --implicit-check-not=__gnat_rcheck_CE_Overflow_Check

-- Check divisor checks, signed remainders and unsigned modular lowering.

-- MLIR:      ada.binop "mod" %{{.*}}, %{{.*}} checks<division> : (!ada.qual<i8, @mod_rem.byte>, !ada.qual<i8, @mod_rem.byte>) -> !ada.qual<i8, @mod_rem.byte>
-- MLIR:      ada.binop "rem" %{{.*}}, %{{.*}} checks<division> : (!ada.qual<i32, @standard.integer>, !ada.qual<i32, @standard.integer>) -> !ada.qual<i32, @standard.integer>
-- MLIR:      ada.binop "rem" %{{[^ ]+}}, %{{[^ ]+}} : (!ada.qual<i32, @standard.integer>, !ada.qual<i32, @standard.integer>) -> !ada.qual<i32, @standard.integer>
-- MLIR:      ada.binop "mod" %{{.*}}, %{{.*}} checks<division> : (!ada.qual<i32, @standard.integer>, !ada.qual<i32, @standard.integer>) -> !ada.qual<i32, @standard.integer>

-- LLVM-LABEL: define i32 @_ada_mod_rem(
-- LLVM:         icmp eq i32 %[[B:[0-9]+]], 0
-- LLVM:         call void @__gnat_rcheck_CE_Divide_By_Zero
-- LLVM:         %[[NEG1:[0-9]+]] = icmp eq i32 %[[B]], -1
-- LLVM-NEXT:    %[[DIV:[0-9]+]] = select i1 %[[NEG1]], i32 1, i32 %[[B]]
-- LLVM-NEXT:    %[[REM:[0-9]+]] = srem i32 %{{[0-9]+}}, %[[DIV]]
-- LLVM-NEXT:    %[[NZ:[0-9]+]] = icmp ne i32 %[[REM]], 0
-- LLVM-NEXT:    %[[XOR:[0-9]+]] = xor i32 %[[REM]], %[[B]]
-- LLVM-NEXT:    %[[DIFF:[0-9]+]] = icmp slt i32 %[[XOR]], 0
-- LLVM-NEXT:    %[[ADJ:[0-9]+]] = and i1 %[[NZ]], %[[DIFF]]
-- LLVM-NEXT:    %[[SUM:[0-9]+]] = add i32 %[[REM]], %[[B]]
-- LLVM-NEXT:    %[[RES:[0-9]+]] = select i1 %[[ADJ]], i32 %[[SUM]], i32 %[[REM]]
-- LLVM-NEXT:    ret i32 %[[RES]]

-- LLVM-LABEL: define internal i8 @mod_rem__byte_mod(
-- LLVM:         call void @__gnat_rcheck_CE_Divide_By_Zero
-- LLVM-NOT:     select
-- LLVM:         %[[UREM:[0-9]+]] = urem i8
-- LLVM-NEXT:    ret i8 %[[UREM]]

-- LLVM-LABEL: define internal i32 @mod_rem__int_rem(
-- LLVM:         call void @__gnat_rcheck_CE_Divide_By_Zero
-- LLVM:         select i1 %{{[0-9]+}}, i32 1, i32
-- LLVM-NEXT:    %[[SREM:[0-9]+]] = srem i32
-- LLVM-NEXT:    ret i32 %[[SREM]]

-- LLVM-LABEL: define internal i32 @mod_rem__rem_minus_one(
-- LLVM-NOT:     __gnat_rcheck_CE_Divide_By_Zero
-- LLVM:         ret i32

function Mod_Rem (A, B : Integer) return Integer is
   type Byte is mod 256;
   function Byte_Mod (X, Y : Byte) return Byte is
   begin
      return X mod Y;
   end Byte_Mod;
   function Int_Rem (X, Y : Integer) return Integer is
   begin
      return X rem Y;
   end Int_Rem;
   function Rem_Minus_One (X : Integer) return Integer is
   begin
      return X rem (-1);
   end Rem_Minus_One;
begin
   return A mod B;
end Mod_Rem;
