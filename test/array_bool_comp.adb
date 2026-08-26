-- The component type need not be Integer: a Boolean component gives an `i1`
-- machine type and the `@standard.boolean` component symbol, while the index
-- stays Integer.

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- CHECK: ada.type @array_bool_comp.flags : !ada.array<i1[i32 x 4]> = #ada.array_info<component @standard.boolean, dim @standard.integer range 1 to 4>

procedure Array_Bool_Comp is
   type Flags is array (1 .. 4) of Boolean;
begin
   null;
end Array_Bool_Comp;
