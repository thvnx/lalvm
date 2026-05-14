-- RUN: %lalvm --emit=mlir --mlir-print-debuginfo --mlir-print-local-scope %s | %FileCheck %s

-- CHECK-LABEL: ada.func @param_nameloc(
-- CHECK-SAME:    %arg0: i32 loc("A"(
-- CHECK-SAME:    %arg1: i32 loc("B"(

function Param_NameLoc (A, B : Integer) return Integer is
begin
   return A + B;
end Param_NameLoc;
