-- RUN: %lalvm --emit=mlir %s | %FileCheck %s
-- XFAIL: *

-- Array-of-array is not supported and component is diagnosed until supported.

-- CHECK: ada.type @array_nonscalar.outer

procedure Array_Nonscalar is
   type Inner is array (1 .. 3) of Integer;
   type Outer is array (1 .. 3) of Inner;
begin
   null;
end Array_Nonscalar;
