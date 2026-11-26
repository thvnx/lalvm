-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- Libadalang 26 resolves the modular negation result to universal integer.
-- The expected type from its context supplies M10.

-- CHECK-LABEL: ada.subp private @unary_static_mod.minus_one_mod(
-- CHECK:         [[ONE:%[0-9]+]] = ada.constant : !ada.qual<i8, @unary_static_mod.m10> = 1
-- CHECK:         ada.unop "-" [[ONE]] : (!ada.qual<i8, @unary_static_mod.m10>) -> !ada.qual<i8, @unary_static_mod.m10>

procedure Unary_Static_Mod is
   type M10 is mod 10;
   function Minus_One_Mod return M10 is (-1);
begin
   null;
end Unary_Static_Mod;
