-- RUN: %lalvm --emit=mlir %s | %FileCheck %s
-- XFAIL: *

-- Unconstrained arrays are not supported, today the `<>` index is diagnosed
-- until support is added.

-- CHECK: ada.type @array_unconstrained.vec

procedure Array_Unconstrained is
   type Vec is array (Integer range <>) of Integer;
begin
   null;
end Array_Unconstrained;
