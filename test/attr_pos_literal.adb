-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --implicit-check-not=.pos

-- CHECK: %[[POS:.*]] = ada.constant : !ada.qual<i128, @standard.universal_int_type_> = 1
-- CHECK-NEXT: %[[RESULT:.*]] = ada.convert implicit %[[POS]]
-- CHECK-NEXT: ada.return %[[RESULT]]

function Attr_Pos_Literal return Integer is
   type Color is (Red, Green);
   for Color use (Red => 10, Green => 42);
begin
   return Color'Pos (Green);
end Attr_Pos_Literal;
