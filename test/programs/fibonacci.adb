-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- REQUIRES: cc
-- RUN: %lalvm --emit=obj %s -o %t.o
-- RUN: %lalvm --bind %s -o %t.b.o
-- RUN: %cc %t.o %t.b.o %gnatstub -o %t
-- RUN: %t 2> %t.err; test $? -eq 1
-- RUN: %FileCheck %s < %t.err

-- Fibonacci numbers until F (47) overflows (RM 11.5), as under GNAT.

function Fibonacci return Integer is
   Previous : Integer := 0;
   Current  : Integer := 1;
   Next     : Integer;
begin
   for I in 2 .. 50 loop
      -- CHECK: raised CONSTRAINT_ERROR : fibonacci.adb:[[#@LINE+1]] overflow check failed
      Next := Previous + Current;
      Previous := Current;
      Current := Next;
   end loop;
   return Current;
end Fibonacci;
