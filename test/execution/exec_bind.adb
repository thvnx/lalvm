-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- REQUIRES: cc
-- RUN: %lalvm --emit=obj %s -o %t.o
-- RUN: %lalvm --bind %s -o %t.b.o
-- RUN: %cc %t.o %t.b.o %gnatstub -o %t
-- RUN: %t; test $? -eq 42

-- End-to-end without GNAT: the unit's object and the bind object linked by the
-- C compiler (with the GNAT stub until LALVM's own runtime is ready).

function Exec_Bind return Integer is
   Sum : Integer := 0;
   I   : Integer := 1;
begin
   while I <= 6 loop
      Sum := Sum + I;
      I := I + 1;
   end loop;
   return Sum * 2;
end Exec_Bind;
