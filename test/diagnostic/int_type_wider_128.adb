-- RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

-- A signed integer type whose range needs more than 128 bits is diagnosed:
-- Libadalang parses and resolves the declaration without complaint (the
-- base-range legality bound is a GNAT concern, not a name-resolution one),
-- so the width derivation in MLIRGen is the only guard.

-- CHECK: error: unsupported integer type wider than 128 bits

procedure Int_Type_Wider_128 is
   type Big is range -2**150 .. 2**150;
   X : Big := 0;
begin
   null;
end Int_Type_Wider_128;
