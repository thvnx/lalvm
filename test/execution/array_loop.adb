-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project
--
-- Array elements indexed by a loop parameter: filled with 2, 4, .., 10 in a
-- first loop, summed in a second one, the sum 30 becomes the exit status.

-- REQUIRES: cc
-- RUN: %lalvm --emit=obj %s -o %t.o
-- RUN: %lalvm --bind %s -o %t.b.o
-- RUN: %cc %t.o %t.b.o %gnatstub -o %t
-- RUN: %check-exit %t 30

function Array_Loop return Integer is
   type Vec is array (1 .. 5) of Integer;
   V   : Vec;
   Sum : Integer := 0;
begin
   for I in 1 .. 5 loop
      V (I) := I * 2;
   end loop;
   for I in 1 .. 5 loop
      Sum := Sum + V (I);
   end loop;
   return Sum;
end Array_Loop;
