-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- A modulus that does not fit uint64 (2**64) now compiles: the modulus is
-- stored as an APInt rather than narrowed. 2**64 distinct values fit in i64;
-- the in-range literal 0 passes the modular range check.
-- CHECK: ada.type @{{.*}} : i64 = #ada.int_info<mod 18446744073709551616>
procedure Mod_64 is
   type U64 is mod 18446744073709551616;
   X : U64 := 0;
begin
   null;
end Mod_64;
