-- RUN: %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

-- A zero-argument function call written without parentheses (e.g.
-- "F : Float := G") is an identifier that p_is_call reports as a call (RM 6.4);
-- it must lower to a call, not be rejected as a value.

-- CHECK-NOT: error: cannot use 'G' as a value
-- CHECK: ada.call @func_call_no_parens.g() : () -> !ada.qual<f32, @standard.float>

procedure Func_Call_No_Parens is
   function G return Float is
   begin
      return 1.0;
   end G;

   F : Float := G;
begin
   null;
end Func_Call_No_Parens;
