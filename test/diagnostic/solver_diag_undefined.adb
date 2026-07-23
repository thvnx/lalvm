-- RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

-- CHECK: solver_diag_undefined.adb:8:11: error: "Undef" is undefined

function Solver_Diag_Undefined return Integer is
   I : Integer := 12;
begin
   return Undef (I);
end Solver_Diag_Undefined;
