-- RUN: %lalvm --emit=mlir %s | %FileCheck %s
-- XFAIL: *

-- Ada 2022 iterator filters (`when`, RM 5.5) are diagnosed, not lowered.
-- XFAIL until filters land; the XPASS then flags the check for finalizing.

-- CHECK-LABEL: ada.subp @for_loop_iter_filter

pragma Ada_2022;
function For_Loop_Iter_Filter return Integer is
   Sum : Integer := 0;
begin
   for I in 1 .. 10 when I mod 2 = 0 loop
      Sum := Sum + I;
   end loop;
   return Sum;
end For_Loop_Iter_Filter;
