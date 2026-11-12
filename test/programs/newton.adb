-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- REQUIRES: cc
-- RUN: %not %lalvm --emit=obj %s -o %t.o 2>&1 | %FileCheck %s --check-prefix=MISSING

-- EXEC: %lalvm --emit=obj %s -o %t.o
-- EXEC: %lalvm --bind %s -o %t.b.o
-- EXEC: %cc %t.o %t.b.o %gnatstub -o %t
-- EXEC: %t; test $? -eq 145

-- Square roots by Newton's method on Float (RM 3.5.7), converted back and
-- forth with Integer (RM 4.6).
-- Missing today: conversions between integer and floating point types.

function Newton return Integer is
   function Sqrt (X : Float) return Float is
      Guess : Float := X;
   begin
      for I in 1 .. 20 loop
         Guess := (Guess + X / Guess) / 2.0;
      end loop;
      return Guess;
   end Sqrt;
begin
   --  141.42 rounds to 141, and 3.5 to 4.
   -- MISSING: newton.adb:[[#@LINE+1]]:11: error: unsupported type conversion
   return Integer (100.0 * Sqrt (2.0)) + Integer (Float (7) / 2.0);
end Newton;
