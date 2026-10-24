-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- Comparison operands, on the base type.

-- CHECK: %[[Y:.*]] = memref.load
-- CHECK-NEXT: ada.cmp "=" %[[Y]], %arg0 : (!ada.qual<i32, @comparison_operand.small>, !ada.qual<i32, @standard.integer>) -> !ada.qual<i1, @standard.boolean>

function Comparison_Operand (I : Integer) return Boolean is
   subtype Small is Integer range 1 .. 10;
   Y : Small := 5;
begin
   return Y = I;
end Comparison_Operand;
