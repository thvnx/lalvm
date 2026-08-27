-- RUN: %lalvm --emit=mlir %s 2>&1 | %FileCheck %s
-- XFAIL: *

-- Initializing an array object is not supported yet.

-- CHECK-NOT: error: array aggregates are not supported

procedure Array_Init is
   type Vec is array (1 .. 3) of Integer;
   V : Vec := (others => 0);
begin
   null;
end Array_Init;
