-- RUN: %lalvm --emit=mlir %s 2>&1 | %FileCheck %s
-- XFAIL: *

-- Unconstrained arrays are not supported, today the `<>` index is diagnosed
-- until support is added.

-- CHECK-NOT: error: unconstrained array types are not supported

procedure Array_Unconstrained is
   type Vec is array (Integer range <>) of Integer;
begin
   null;
end Array_Unconstrained;
