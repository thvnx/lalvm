-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- REQUIRES: cc
-- RUN: %lalvm --emit=obj %s -o %t.o
-- RUN: %lalvm --bind %s -o %t.b.o
-- RUN: %cc %t.o %t.b.o %gnatstub -o %t
-- RUN: %t; test $? -eq 30

-- After Lovelace's Note G: B(8) = -1/30 by the Akiyama-Tanigawa scheme.

function Exec_Note_G return Integer is
   type Frac_Array is array (0 .. 8) of Integer;
   Num     : Frac_Array;
   Den     : Frac_Array;
   N, D, G : Integer;

   function GCD (A, B : Integer) return Integer is
      X : Integer := A;
      Y : Integer := B;
   begin
      while X /= Y loop
         if X > Y then
            X := X - Y;
         else
            Y := Y - X;
         end if;
      end loop;
      return X;
   end GCD;
begin
   for M in 0 .. 8 loop
      Num (M) := 1;
      Den (M) := M + 1;
      for J in reverse 1 .. M loop
         -- A (J - 1) := J * (A (J - 1) - A (J)) on Num / Den.
         N := J * (Num (J - 1) * Den (J) - Num (J) * Den (J - 1));
         D := Den (J - 1) * Den (J);
         if N = 0 then
            D := 1;
         else
            G := GCD (abs N, D);
            N := N / G;
            D := D / G;
         end if;
         Num (J - 1) := N;
         Den (J - 1) := D;
      end loop;
   end loop;
   return (if Num (0) = -1 then Den (0) else 0);
end Exec_Note_G;
