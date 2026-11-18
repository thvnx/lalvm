-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- REQUIRES: cc
-- RUN: %lalvm -O0 --emit=obj %s -o %t.o
-- RUN: %lalvm --bind %s -o %t.b.o
-- RUN: %cc %t.o %t.b.o %gnatstub -lm -o %t
-- RUN: %check-exit %t 145
-- RUN: %lalvm -O1 --emit=obj %s -o %t.o
-- RUN: %cc %t.o %t.b.o %gnatstub -lm -o %t
-- RUN: %check-exit %t 145

-- Square roots by Newton's method on Float (RM 3.5.7), converted back and
-- forth with Integer (RM 4.6).

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
   return Integer (100.0 * Sqrt (2.0)) + Integer (Float (7) / 2.0);
end Newton;
