-- A statically-constrained 1-D array type declaration emits an `ada.type`
-- carrying its `!ada.array` layout and `array_info` metadata.

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- CHECK: ada.type @array_decl.vec : !ada.array<i32[i32 x 10]> = #ada.array_info<component @standard.integer, dim @standard.integer range 1 to 10>

procedure Array_Decl is
   type Vec is array (1 .. 10) of Integer;
begin
   null;
end Array_Decl;
