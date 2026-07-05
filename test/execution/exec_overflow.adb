-- REQUIRES: gnat
-- RUN: rm -rf %t.dir && mkdir -p %t.dir && cp %s %t.dir/exec_overflow.adb && cd %t.dir && env LALVM=%lalvm %lalvm-make exec_overflow.adb
-- RUN: %not %t.dir/exec_overflow > %t.out 2>&1
-- RUN: %FileCheck %s < %t.out

-- A * B overflows Integer; lalvm's overflow check traps via the GNAT runtime.
-- CHECK: raised CONSTRAINT_ERROR : exec_overflow.adb:11 overflow check failed
procedure Exec_Overflow is
   A : Integer := 100_000;
   B : Integer := 100_000;
   C : Integer := A * B;
begin
   null;
end Exec_Overflow;
