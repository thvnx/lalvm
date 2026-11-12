-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- REQUIRES: cc
-- RUN: %not %lalvm --emit=obj %s -o %t.o 2>&1 | %FileCheck %s --check-prefix=MISSING

-- EXEC: %lalvm --emit=obj %s -o %t.o
-- EXEC: %lalvm --bind %s -o %t.b.o
-- EXEC: %cc %t.o %t.b.o %gnatstub -o %t
-- EXEC: %t; test $? -eq 69

-- The harmonic number H (6) = 49 / 20 reduced by Euclid's algorithm, then `mod`
-- and `rem` on a negative operand (RM 4.5.5).
-- Missing today: `mod` and `rem`.

function Gcd return Integer is
   function Gcd (A, B : Natural) return Natural is
   begin
      if B = 0 then
         return A;
      end if;
      -- MISSING: gcd.adb:[[#@LINE+1]]:24: error: invalid binary operator '<OpRem "rem"
      return Gcd (B, A rem B);
   end Gcd;

   Num : Positive := 1;
   Den : Positive := 1;
   G   : Positive;
begin
   for K in 2 .. 6 loop
      Num := Num * K + Den;
      Den := Den * K;
      G := Gcd (Num, Den);
      Num := Num / G;
      Den := Den / G;
   end loop;
   --  A = (A / B) * B + A rem B, and A mod B takes the sign of B:
   --  -49 = -2 * 20 - 9 = -3 * 20 + 11.
   if -Num /= ((-Num) / Den) * Den + (-Num) rem Den or (-Num) mod Den /= 11
   then
      return 0;
   end if;
   return Num + Den;
end Gcd;
