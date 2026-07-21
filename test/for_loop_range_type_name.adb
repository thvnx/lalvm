-- RUN: %lalvm --emit=mlir %s | %FileCheck %s
-- XFAIL: *

-- A range given by a type name (`for I in Boolean`, RM 5.5) is diagnosed: it
-- needs 'First/'Last as the bounds. XFAIL until this form lands.

-- CHECK-LABEL: ada.subp @for_loop_range_type_name

function For_Loop_Range_Type_Name return Integer is
   Count : Integer := 0;
begin
   for I in Boolean loop
      Count := Count + 1;
   end loop;
   return Count;
end For_Loop_Range_Type_Name;
