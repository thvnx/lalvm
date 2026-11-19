-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project
--
-- Array elements end to end, with a lower bound far from 0 so the `'First`
-- normalization matters: written through literal indices, read back through a
-- literal and a variable index, the sum 42 becomes the exit status.

-- REQUIRES: cc
-- RUN: %lalvm --emit=obj %s -o %t.o
-- RUN: %lalvm --bind %s -o %t.b.o
-- RUN: %cc %t.o %t.b.o %gnatstub -o %t
-- RUN: %check-exit %t 42


function Array_Elements return Integer is
   type Vec is array (5 .. 14) of Integer;
   V : Vec;
   I : Integer := 14;
begin
   V (5) := 40;
   V (14) := 2;
   return V (5) + V (I);
end Array_Elements;
