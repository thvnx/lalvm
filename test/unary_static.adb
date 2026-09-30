-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s
-- XFAIL: libadalang-26

-- A static unary expression (RM 4.9) folds to a single `ada.constant`, with no
-- `ada.unop`. A modular one is not folded yet: Libadalang's `eval_as_int` does
-- not wrap each modular operation (RM 3.5.4(19)).

-- CHECK-LABEL: ada.subp private @unary_static.minus_one(
-- CHECK-NOT:     ada.unop
-- CHECK:         ada.constant : !ada.qual<i32, @standard.integer> = -1

-- CHECK-LABEL: ada.subp private @unary_static.plus_three(
-- CHECK-NOT:     ada.unop
-- CHECK:         ada.constant : !ada.qual<i32, @standard.integer> = 3

-- CHECK-LABEL: ada.subp private @unary_static.abs_minus_five(
-- CHECK-NOT:     ada.unop
-- CHECK:         ada.constant : !ada.qual<i32, @standard.integer> = 5

-- CHECK-LABEL: ada.subp private @unary_static.minus_one_mod(
-- CHECK:         [[ONE:%[0-9]+]] = ada.constant : !ada.qual<i8, @unary_static.m10> = 1
-- CHECK:         ada.unop "-" [[ONE]] : !ada.qual<i8, @unary_static.m10>

procedure Unary_Static is
   type M10 is mod 10;

   function Minus_One return Integer is (-1);
   function Plus_Three return Integer is (+3);
   function Abs_Minus_Five return Integer is (abs (-5));
   function Minus_One_Mod return M10 is (-1);
begin
   null;
end Unary_Static;
