-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- Integer `/` carries `checks<division>` (RM 11.5) for both signed and modular
-- types (zero divisor; and Integer'First / -1 for signed). Float `/` is not
-- flagged. The nested modular and float ops are emitted before the outer one.
-- CHECK:      ada.binop "/" %{{.*}}, %{{.*}} checks<division> : (!ada.qual<i8, @binop_division.byte>, !ada.qual<i8, @binop_division.byte>) -> !ada.qual<i8, @binop_division.byte>
-- CHECK:      ada.binop "/" %{{.*}}, %{{.*}} : (!ada.qual<f32, @standard.float>, !ada.qual<f32, @standard.float>) -> !ada.qual<f32, @standard.float>
-- CHECK:      ada.binop "/" %{{.*}}, %{{.*}} checks<division> : (!ada.qual<i32, @standard.integer>, !ada.qual<i32, @standard.integer>) -> !ada.qual<i32, @standard.integer>

function Binop_Division (A, B : Integer) return Integer is
   type Byte is mod 256;
   function Wrap (X, Y : Byte) return Byte is
   begin
      return X / Y;
   end Wrap;
   function FDiv (F, G : Float) return Float is
   begin
      return F / G;
   end FDiv;
begin
   return A / B;
end Binop_Division;
