-- XFAIL: *
-- RUN: %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

-- Known limitation: zero-argument function calls written without parentheses
-- (e.g. "F : Float := G") are not recognized as calls. Use p_is_call to
-- detect this case in mlirGenVariable.

-- CHECK-NOT: error: cannot use 'G' as a value

procedure Func_Call_No_Parens is
   function G return Float is
   begin
      return 1.0;
   end G;

   F : Float := G;
begin
   null;
end Func_Call_No_Parens;
