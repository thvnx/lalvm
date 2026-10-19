-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- REQUIRES: cc
-- RUN: %lalvm --emit=obj %s -o %t.o
-- RUN: %lalvm --bind %s -o %t.b.o
-- RUN: %cc %t.o %t.b.o %gnatstub -o %t
-- RUN: %t; test $? -eq 57

-- Order sensitive: I = 3, 2, 1 gives 57, the forward order would give 27.

function Exec_For_Loop_Reverse return Integer is
   R : Integer := 0;
begin
   for I in reverse 1 .. 3 loop
      R := R * 4 + I;
   end loop;
   return R;
end Exec_For_Loop_Reverse;
