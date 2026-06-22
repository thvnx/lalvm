-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --implicit-check-not=__gnat_rcheck_CE_Divide_By_Zero --implicit-check-not=__gnat_rcheck_CE_Overflow_Check

-- A static nonzero divisor resolves the Division_Check at compile time
-- (RM 11.5), GNAT-style: neither the zero-divisor check nor the
-- `Integer'First / -1` overflow precheck can fire, so neither is emitted (the
-- `--implicit-check-not` lines guard that). The signed divide remains.
-- CHECK-LABEL: define i32 @_ada_division_check_static
-- CHECK:         sdiv i32

function Division_Check_Static (A : Integer) return Integer is
begin
   return A / 2;
end Division_Check_Static;
