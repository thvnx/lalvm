-- RUN: %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

procedure Param_Out_Read_Warn is
   procedure P (A : out Integer) is
      -- CHECK: param_out_read_warn.adb:[[#@LINE+1]]:22: warning: {{.*}}'A' is read before first assignment
      X : Integer := A;
   begin
      A := X;
   end P;
begin
   null;
end Param_Out_Read_Warn;
