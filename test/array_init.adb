-- RUN: %lalvm --emit=mlir %s | %FileCheck %s
-- XFAIL: *

-- Initializing an array object is not supported yet.

-- CHECK: memref.store

procedure Array_Init is
   type Vec is array (1 .. 3) of Integer;
   V : Vec := (others => 0);
begin
   null;
end Array_Init;
