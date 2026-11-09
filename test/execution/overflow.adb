-- REQUIRES: cc
-- RUN: %lalvm --emit=obj %s -o %t.o
-- RUN: %lalvm --bind %s -o %t.b.o
-- RUN: %cc %t.o %t.b.o %gnatstub -o %t
-- RUN: %not %t > %t.out 2>&1
-- RUN: %FileCheck %s < %t.out

-- A * B overflows Integer; lalvm's overflow check traps via the GNAT runtime.

procedure Overflow is
   A : Integer := 100_000;
   B : Integer := 100_000;
   -- CHECK: raised CONSTRAINT_ERROR : overflow.adb:[[#@LINE+1]] overflow check failed
   C : Integer := A * B;
begin
   null;
end Overflow;
