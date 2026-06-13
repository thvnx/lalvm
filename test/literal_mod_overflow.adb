-- RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

-- A modular type's static range is its implied 0 .. modulus-1, derived from
-- the modulus, so a literal outside it fails the Constraint_Check.
-- CHECK: error: value not in range of type "{{.*}}byte"
-- CHECK: error: static expression fails Constraint_Check

procedure Literal_Mod_Overflow is
   type Byte is mod 256;
   B : Byte := 300;
begin
   null;
end Literal_Mod_Overflow;
