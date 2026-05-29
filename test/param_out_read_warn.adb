-- XFAIL: *
-- RUN: %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

-- Known limitation: warning for reading an uninitialized out parameter uses
-- "variable" and "read before first assignment". Should say something like
-- "formal parameter 'A' may be referenced before it has a value".

-- CHECK-NOT: warning: variable 'A' is read before first assignment

procedure Param_Out_Read_Warn is
   procedure P (A : out Integer) is
      X : Integer := A;
   begin
      A := X;
   end P;
begin
   null;
end Param_Out_Read_Warn;
