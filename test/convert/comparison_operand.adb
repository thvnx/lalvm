-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- Comparison operands, on the base type.

-- CHECK: ada.coerce %{{.*}} : <i32, @comparison_operand.small> to <i32, @standard.integer>
-- CHECK: ada.cmp "="

function Comparison_Operand (I : Integer) return Boolean is
   subtype Small is Integer range 1 .. 10;
   Y : Small := 5;
begin
   return Y = I;
end Comparison_Operand;
