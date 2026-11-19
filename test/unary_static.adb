-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- CHECK-LABEL: ada.subp private @unary_static.minus_one(
-- CHECK-NOT:     ada.unop
-- CHECK:         ada.constant : !ada.qual<i32, @standard.integer> = -1

-- CHECK-LABEL: ada.subp private @unary_static.plus_three(
-- CHECK-NOT:     ada.unop
-- CHECK:         ada.constant : !ada.qual<i32, @standard.integer> = 3

-- CHECK-LABEL: ada.subp private @unary_static.abs_minus_five(
-- CHECK-NOT:     ada.unop
-- CHECK:         ada.constant : !ada.qual<i32, @standard.integer> = 5

procedure Unary_Static is
   function Minus_One return Integer is (-1);
   function Plus_Three return Integer is (+3);
   function Abs_Minus_Five return Integer is (abs (-5));
begin
   null;
end Unary_Static;
