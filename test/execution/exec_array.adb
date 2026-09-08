-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project
--
-- Array elements end to end, with a lower bound far from 0 so the `'First`
-- normalization matters: written through literal indices, read back through a
-- literal and a variable index, the sum 42 becomes the exit status.

-- REQUIRES: gnat
-- RUN: rm -rf %t.dir && mkdir -p %t.dir && cp %s %t.dir/exec_array.adb && cd %t.dir && env LALVM=%lalvm %lalvm-make exec_array.adb
-- RUN: %t.dir/exec_array; test $? -eq 42


function Exec_Array return Integer is
   type Vec is array (5 .. 14) of Integer;
   V : Vec;
   I : Integer := 14;
begin
   V (5) := 40;
   V (14) := 2;
   return V (5) + V (I);
end Exec_Array;
