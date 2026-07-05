-- REQUIRES: gnat
-- RUN: rm -rf %t.dir && mkdir -p %t.dir && cp %s %t.dir/exec_smoke.adb && cd %t.dir && env LALVM=%lalvm %lalvm-make exec_smoke.adb
-- RUN: %t.dir/exec_smoke

-- End to end: a loop runs to completion and the program exits 0.

procedure Exec_Smoke is
   Sum : Integer := 0;
   I   : Integer := 1;
begin
   while I <= 10 loop
      Sum := Sum + I;
      I := I + 1;
   end loop;
end Exec_Smoke;
