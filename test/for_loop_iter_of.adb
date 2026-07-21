-- RUN: %lalvm --emit=mlir %s | %FileCheck %s
-- XFAIL: *

-- `for ... of` iteration (RM 5.5.2) needs array support; today the anonymous
-- array type is diagnosed. XFAIL until arrays and `for ... of` land.

-- CHECK-LABEL: ada.subp @for_loop_iter_of

function For_Loop_Iter_Of return Integer is
   A   : array (1 .. 3) of Integer := (10, 20, 30);
   Sum : Integer := 0;
begin
   for E of A loop
      Sum := Sum + E;
   end loop;
   return Sum;
end For_Loop_Iter_Of;
