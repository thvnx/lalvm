-- RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

-- CHECK: error: digits value out of range, maximum is 18

procedure Digits_Out_Of_Range is
   type Quad is digits 33;
begin
   null;
end Digits_Out_Of_Range;
