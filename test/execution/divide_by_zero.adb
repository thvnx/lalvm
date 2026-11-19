-- REQUIRES: cc
-- RUN: %lalvm --emit=obj %s -o %t.o
-- RUN: %lalvm --bind %s -o %t.b.o
-- RUN: %cc %t.o %t.b.o %gnatstub -o %t
-- RUN: %not %t > %t.out 2>&1
-- RUN: %FileCheck %s < %t.out

-- 100 / N with N = 0 at run time traps via the GNAT runtime.
procedure Divide_By_Zero is
   N : Integer := 0;
   -- CHECK: raised CONSTRAINT_ERROR : divide_by_zero.adb:[[#@LINE+1]] divide by zero
   Q : Integer := 100 / N;
begin
   null;
end Divide_By_Zero;
