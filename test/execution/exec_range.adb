-- REQUIRES: gnat
-- RUN: rm -rf %t.dir && mkdir -p %t.dir && cp %s %t.dir/exec_range.adb && cd %t.dir && env LALVM=%lalvm %lalvm-make exec_range.adb
-- RUN: %not %t.dir/exec_range > %t.out 2>&1
-- RUN: %FileCheck %s < %t.out

-- V = 20 assigned to Small (1 .. 10) fails the range check at run time.
-- CHECK: raised CONSTRAINT_ERROR : exec_range.adb:11 range check failed
procedure Exec_Range is
   subtype Small is Integer range 1 .. 10;
   V : Integer := 20;
   X : Small := V;
begin
   null;
end Exec_Range;
