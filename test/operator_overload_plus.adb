-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- User-defined unary "+" (RM 6.6): the definition's GNAT mangling is `Oplus`
-- (a single-parameter "+", vs `Oadd` for the binary form).

-- MLIR: ada.subp private @"operator_overload_plus.+"(%{{.*}}: !ada.qual<i32, @standard.integer>) -> !ada.qual<i32, @standard.integer>

-- LLVM-LABEL: define internal i32 @operator_overload_plus__Oplus(
-- LLVM:          ret i32

procedure Operator_Overload_Plus is
   function "+" (A : Integer) return Integer is
   begin
      return A;
   end "+";
begin
   null;
end Operator_Overload_Plus;
