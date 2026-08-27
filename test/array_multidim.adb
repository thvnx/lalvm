-- RUN: %lalvm --emit=mlir %s 2>&1 | %FileCheck %s
-- XFAIL: *

-- The extra dimension is diagnosed until multidimensional arrays are supported.

-- CHECK-NOT: error: multi-dimensional array types are not supported

procedure Array_Multidim is
   type Mat is array (1 .. 3, 1 .. 4) of Integer;
begin
   null;
end Array_Multidim;
