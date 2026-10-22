-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- Arithmetic operands. A converted static divisor drops the Division_Check.

-- CHECK: ada.coerce %{{.*}} : <i32, @arithmetic_operand.small> to <i32, @standard.integer>
-- CHECK: %[[TWO:.*]] = ada.coerce %{{.*}} : <i512, @standard.universal_int_type_> to <i32, @standard.integer>
-- CHECK: ada.binop "/" %{{.*}}, %[[TWO]] : !ada.qual<i32, @standard.integer>

function Arithmetic_Operand (I : Integer) return Integer is
   subtype Small is Integer range 1 .. 10;
   Two : constant := 2;
   Y : Small := 5;
begin
   return (Y + I) / Two;
end Arithmetic_Operand;
