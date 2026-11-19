-- RUN: %lalvm --emit=mlir %s | %FileCheck %s
-- XFAIL: *

-- CHECK-LABEL: ada.subp @for_loop_iter_of

function For_Loop_Iter_Of return Integer is
   type Vec is array (1 .. 3) of Integer;
   A : Vec;
   Sum : Integer := 0;
begin
   A (1) := 10;
   A (2) := 20;
   A (3) := 30;
   for E of A loop
      Sum := Sum + E;
   end loop;
   return Sum;
end For_Loop_Iter_Of;
