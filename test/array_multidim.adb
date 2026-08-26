-- RUN: %lalvm --emit=mlir %s | %FileCheck %s
-- XFAIL: *

-- The extra dimension is diagnosed until multidimensional arrays are supported.

-- CHECK: ada.type @array_multidim.mat

procedure Array_Multidim is
   type Mat is array (1 .. 3, 1 .. 4) of Integer;
begin
   null;
end Array_Multidim;
