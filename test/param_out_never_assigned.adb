-- XFAIL: *
-- RUN: %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

-- Known limitation: lalvm does not detect that an out parameter is read but
-- never assigned within the subprogram body. Requires whole-body analysis
-- (e.g. at verify level) to emit "formal parameter 'A' is read but never
-- assigned" as GCC does.

-- CHECK-NOT: warning: variable 'A' is read before first assignment

procedure Param_Out_Never_Assigned is
   procedure P (A, B : out Integer) is
      X : Integer := A;
   begin
      B := X;
   end P;
begin
   null;
end Param_Out_Never_Assigned;
