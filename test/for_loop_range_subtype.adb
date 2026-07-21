-- RUN: %lalvm --emit=mlir %s | %FileCheck %s
-- XFAIL: *

-- A subtype_indication range (`T range L .. H`, RM 5.5) is diagnosed: only a
-- bare `L .. H` is lowered. XFAIL until this form lands.

-- CHECK-LABEL: ada.subp @for_loop_range_subtype

function For_Loop_Range_Subtype return Integer is
   Count : Integer := 0;
begin
   for I in Integer range 1 .. 5 loop
      Count := Count + 1;
   end loop;
   return Count;
end For_Loop_Range_Subtype;
