-- RUN: %lalvm --emit=mlir %s 2>&1 | %FileCheck %s
-- XFAIL: *

-- Array-of-array is not supported and component is diagnosed until supported.

-- CHECK-NOT: error: array component type must be scalar

procedure Array_Nonscalar is
   type Inner is array (1 .. 3) of Integer;
   type Outer is array (1 .. 3) of Inner;
begin
   null;
end Array_Nonscalar;
