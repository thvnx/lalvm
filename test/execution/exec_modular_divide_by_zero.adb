-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- REQUIRES: cc
-- RUN: %lalvm --emit=obj %s -o %t.o
-- RUN: %lalvm --bind %s -o %t.b.o
-- RUN: %cc %t.o %t.b.o %gnatstub -o %t
-- RUN: %not %t 2> %t.out
-- RUN: %FileCheck %s < %t.out

-- A zero divisor in a non-binary modular `/` raises Constraint_Error (RM 11.5).

procedure Exec_Modular_Divide_By_Zero is
   type Kilo is mod 1000;

   function Div_K (X, Y : Kilo) return Kilo is
   begin
      -- CHECK: raised CONSTRAINT_ERROR : exec_modular_divide_by_zero.adb:[[#@LINE+1]] divide by zero
      return X / Y;
   end Div_K;

   K : Kilo := 700;
begin
   K := Div_K (K, 7);
   K := Div_K (K, 0);
end Exec_Modular_Divide_By_Zero;
