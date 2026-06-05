-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- A parameterless procedure call written without parentheses ("P;") is a call
-- statement whose name is a bare identifier; it lowers to a procedure call
-- (no result).

-- CHECK: ada.call @proc_call_no_parens.p() : () -> ()

procedure Proc_Call_No_Parens is
   procedure P is
   begin
      null;
   end P;
begin
   P;
end Proc_Call_No_Parens;
