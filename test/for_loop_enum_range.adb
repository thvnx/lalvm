-- RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

-- A non-integer range (RM 5.5) is diagnosed rather than miscompiled: stepping
-- on the representation (1,2,4,8) would not iterate the four positions.

-- CHECK: error: `for` loop over a non-integer range is not yet supported

function For_Loop_Enum_Range return Integer is
   type Bits is (B0, B1, B2, B3);
   for Bits use (B0 => 1, B1 => 2, B2 => 4, B3 => 8);
   Count : Integer := 0;
begin
   for B in B0 .. B3 loop
      Count := Count + 1;
   end loop;
   return Count;
end For_Loop_Enum_Range;
