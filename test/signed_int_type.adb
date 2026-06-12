-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- The width derives from the declared range: -100 .. 100 fits a signed
-- byte, so Small is i8.
-- MLIR: ada.constant : !ada.qual<i8, @signed_int_type.small> = 42
-- MLIR: ada.type @signed_int_type.small : i8 = #ada.int_info<range -100 to 100>

-- LLVM-LABEL: define void @_ada_signed_int_type(

procedure Signed_Int_Type is
   type Small is range -100 .. 100;
   X : Small := 42;
begin
   null;
end Signed_Int_Type;
