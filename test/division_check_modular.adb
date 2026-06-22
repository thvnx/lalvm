-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --implicit-check-not=__gnat_rcheck_CE_Overflow_Check

-- Modular `/` (RM 11.5, RM 3.5.4) is unsigned and wraps, so it carries only the
-- zero-divisor Division_Check, never the signed `Integer'First / -1` overflow
-- precheck (the `--implicit-check-not` guards that no overflow raise is
-- emitted). The divide is unsigned and follows the zero check.

-- CHECK-LABEL: define {{.*}} @division_check_modular__div_k
-- CHECK:         icmp eq i16 %{{.*}}, 0
-- CHECK:         call void @__gnat_rcheck_CE_Divide_By_Zero
-- CHECK:         udiv i16

procedure Division_Check_Modular is
   type Kilo is mod 1000;

   function Div_K (X, Y : Kilo) return Kilo is
   begin
      return X / Y;
   end Div_K;

   K : Kilo := 700;
begin
   K := Div_K (K, K);
end Division_Check_Modular;
