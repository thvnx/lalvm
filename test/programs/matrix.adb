-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- REQUIRES: cc
-- RUN: %not %lalvm --emit=obj %s -o %t.o 2>&1 | %FileCheck %s --check-prefix=MISSING

-- EXEC: %lalvm --emit=obj %s -o %t.o
-- EXEC: %lalvm --bind %s -o %t.b.o
-- EXEC: %cc %t.o %t.b.o %gnatstub -o %t
-- EXEC: %t; test $? -eq 144

-- Fibonacci by matrix power: a two-dimensional array type (RM 3.6) returned by
-- a user-defined "*" (RM 6.5).
-- Missing today: multidimensional arrays, array results, aggregates.

function Matrix return Integer is
   -- MISSING: matrix.adb:[[#@LINE+1]]:4: error: multi-dimensional array types are not supported
   type Matrix is array (1 .. 2, 1 .. 2) of Integer;

   function "*" (A, B : Matrix) return Matrix is
      C : Matrix;
   begin
      for I in A'Range (1) loop
         for J in B'Range (2) loop
            C (I, J) := 0;
            for K in A'Range (2) loop
               C (I, J) := C (I, J) + A (I, K) * B (K, J);
            end loop;
         end loop;
      end loop;
      return C;
   end "*";

   Step  : constant Matrix := ((1, 1), (1, 0));
   Power : Matrix := ((1, 0), (0, 1));
begin
   for I in 1 .. 12 loop
      Power := Power * Step;
   end loop;
   --  Step ** 12 = ((F (13), F (12)), (F (12), F (11))) = ((233, 144), ...).
   if Power (1, 1) /= Power (1, 2) + Power (2, 2) then
      return 0;
   end if;
   return Power (2, 1);
end Matrix;
