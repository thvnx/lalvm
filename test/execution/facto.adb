-- REQUIRES: cc
-- RUN: %lalvm --emit=obj %s -o %t.o
-- RUN: %lalvm --bind %s -o %t.b.o
-- RUN: %cc %t.o %t.b.o %gnatstub -o %t
-- RUN: %check-exit %t 120

-- Factorial end to end: F (5) = 120 becomes the exit status.

function Facto return Integer is
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
end Facto;
