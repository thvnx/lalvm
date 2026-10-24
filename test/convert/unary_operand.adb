-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- Unary operand, on the base type.

-- CHECK: %[[Y:.*]] = memref.load
-- CHECK-NEXT: ada.unop "-" %[[Y]] checks<overflow> : (!ada.qual<i32, @unary_operand.small>) -> !ada.qual<i32, @standard.integer>

function Unary_Operand return Integer is
   subtype Small is Integer range 1 .. 10;
   Y : Small := 5;
begin
   return -Y;
end Unary_Operand;
