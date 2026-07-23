-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- User-defined binary "+" (RM 6.6): the definition's GNAT mangling is `Oadd`
-- (`operator_overload.adb` covers only `Omultiply`).

-- MLIR: ada.subp private @"operator_overload_add.+"(%{{.*}}: !ada.qual<i32, @standard.integer>, %{{.*}}: !ada.qual<i32, @standard.integer>) -> !ada.qual<i32, @standard.integer>

-- LLVM-LABEL: define internal i32 @operator_overload_add__Oadd(
-- LLVM:          ret i32

procedure Operator_Overload_Add is
   function "+" (A : Integer; B : Integer) return Integer is
   begin
      return A - B;
   end "+";
begin
   null;
end Operator_Overload_Add;
