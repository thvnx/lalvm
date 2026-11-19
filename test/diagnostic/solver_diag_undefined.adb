-- RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

function Solver_Diag_Undefined return Integer is
   I : Integer := 12;
begin
   -- CHECK: solver_diag_undefined.adb:[[#@LINE+1]]:11: error: {{no such entity|"Undef" is undefined}}
   return Undef (I);
end Solver_Diag_Undefined;
