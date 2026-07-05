-- REQUIRES: gnat
-- RUN: rm -rf %t.dir && mkdir -p %t.dir && cp %s %t.dir/exec_facto.adb && cd %t.dir && env LALVM=%lalvm %lalvm-make exec_facto.adb
-- RUN: %t.dir/exec_facto; test $? -eq 120

-- Factorial end to end: F (5) = 120 becomes the exit status.

function Exec_Facto return Integer is
   function F (X : Integer) return Integer is
   begin
      if X = 0 then
         return 1;
      else
         return F (X - 1) * X;
      end if;
   end F;
begin
   return F (5);
end Exec_Facto;
