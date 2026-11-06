-- Static array objects retain semantic identity until storage lowering.

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- The object precedes its type declaration in ada.decls.
-- MLIR: ada.object : !ada.ref<!ada.qual<!ada.array<i32[i32 x 10]>, @array_decl.vec>>
-- MLIR: ada.type @array_decl.__anonymous_vec_index_1 base @standard.integer : i32 = #ada.int_info<range 1 to 10>
-- MLIR: ada.type @array_decl.vec : !ada.array<i32[i32 x 10]> = #ada.array_info<component @standard.integer, dim @array_decl.__anonymous_vec_index_1 range 1 to 10>

-- allocate a llvm.array when lowering to LLVM
-- LLVM: alloca [10 x i32]

procedure Array_Decl is
   type Vec is array (1 .. 10) of Integer;
   My_Vec : Vec;
begin
   null;
end Array_Decl;
