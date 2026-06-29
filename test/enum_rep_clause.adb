-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- Enumeration representation clause (RM 13.4): `p_enum_rep` supplies the clause
-- values, and the type is the smallest signed width spanning the whole rep
-- range, not the literal count. Here -1000 .. 99 needs i11 (the negative bound
-- is binding; sizing off the max alone would wrongly pick i8). A negative rep
-- lowers as a signed constant. `C` is read so the literals are not folded away.

-- CHECK-DAG: ada.type @enum_rep_clause.color : i11 = #ada.enum_info<"red" = -1000, "green" = 42, "blue" = 99>
-- CHECK-DAG: ada.constant : !ada.qual<i11, @enum_rep_clause.color> = -1000
-- CHECK-DAG: ada.constant : !ada.qual<i11, @enum_rep_clause.color> = 99

procedure Enum_Rep_Clause (Match : out Boolean) is
   type Color is (Red, Green, Blue);
   for Color use (Red => -1000, Green => 42, Blue => 99);
   C : Color := Red;
begin
   Match := C = Blue;
end Enum_Rep_Clause;
