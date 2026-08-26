-- A statically-constrained array with a negative lower bound: the extent is
-- the element count 5 - (-5) + 1 = 11, and the bounds are recorded as written.

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- CHECK: ada.type @array_neg_bounds.vec : !ada.array<i32[i32 x 11]> = #ada.array_info<component @standard.integer, dim @standard.integer range -5 to 5>

procedure Array_Neg_Bounds is
   type Vec is array (-5 .. 5) of Integer;
begin
   null;
end Array_Neg_Bounds;
