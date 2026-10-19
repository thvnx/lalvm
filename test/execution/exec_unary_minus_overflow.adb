-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- REQUIRES: cc
-- RUN: %lalvm -O1 --emit=obj %s -o %t.o
-- RUN: %lalvm --bind %s -o %t.b.o
-- RUN: %cc %t.o %t.b.o %gnatstub -o %t
-- RUN: %not %t 2> %t.out
-- RUN: %FileCheck %s < %t.out

-- At -O1, X becomes the constant Integer'First and -X must still raise.

function Exec_Unary_Minus_Overflow return Integer is
   X : Integer := Integer'First;
begin
   -- CHECK: raised CONSTRAINT_ERROR : exec_unary_minus_overflow.adb:[[#@LINE+1]] overflow check failed
   return -X;
end Exec_Unary_Minus_Overflow;
