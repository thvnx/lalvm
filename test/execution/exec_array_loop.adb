-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project
--
-- Array elements indexed by a loop parameter: filled with 2, 4, .., 10 in a
-- first loop, summed in a second one, the sum 30 becomes the exit status.

-- REQUIRES: gnat
-- RUN: rm -rf %t.dir && mkdir -p %t.dir && cp %s %t.dir/exec_array_loop.adb && cd %t.dir && env LALVM=%lalvm %lalvm-make exec_array_loop.adb
-- RUN: %t.dir/exec_array_loop; test $? -eq 30

function Exec_Array_Loop return Integer is
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
end Exec_Array_Loop;
