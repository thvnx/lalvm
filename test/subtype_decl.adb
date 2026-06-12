-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- A subtype's ada.type links its base type and records the declared range.
-- MLIR: ada.type @subtype_decl.small base @standard.integer : i32 = #ada.int_info<range 1 to 100>
-- MLIR-LABEL: ada.subp @subtype_decl
-- MLIR: ada.constant : !ada.qual<i32, @subtype_decl.small> = 42

-- LLVM-LABEL: define void @_ada_subtype_decl(

procedure Subtype_Decl is
   subtype Small is Integer range 1 .. 100;
   X : Small := 42;
begin
   null;
end Subtype_Decl;
