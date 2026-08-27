-- A statically-constrained 1-D array type declaration emits an `ada.type` with
-- its `!ada.array` layout and `array_info`. An object of it allocates a memref
-- cell.

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- ada.alloca is printed first since the type declaration is nested into ada.decls.
-- CHECK: ada.alloca : memref<!ada.qual<!ada.array<i32[i32 x 10]>, @array_decl.vec>>
-- CHECK: ada.type @array_decl.vec : !ada.array<i32[i32 x 10]> = #ada.array_info<component @standard.integer, dim @standard.integer range 1 to 10>

procedure Array_Decl is
   type Vec is array (1 .. 10) of Integer;
   My_Vec : Vec;
begin
   null;
end Array_Decl;
