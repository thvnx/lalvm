-- REQUIRES: gnat
-- RUN: rm -rf %t.dir && mkdir -p %t.dir && cp %s %t.dir/exec_divide_by_zero.adb && cd %t.dir && env LALVM=%lalvm %lalvm-make exec_divide_by_zero.adb
-- RUN: %not %t.dir/exec_divide_by_zero > %t.out 2>&1
-- RUN: %FileCheck %s < %t.out

-- 100 / N with N = 0 at run time traps via the GNAT runtime.
-- CHECK: raised CONSTRAINT_ERROR : exec_divide_by_zero.adb:10 divide by zero
procedure Exec_Divide_By_Zero is
   N : Integer := 0;
   Q : Integer := 100 / N;
begin
   null;
end Exec_Divide_By_Zero;
