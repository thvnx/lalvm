-- RUN: %lalvm --emit=mlir --mlir-print-debuginfo --mlir-print-local-scope %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR: ada.type @standard.boolean : i1 = #ada.enum_info<"false" = 0, "true" = 1>
-- MLIR-LABEL: ada.subp @param_nameloc(
-- MLIR-SAME:    %arg0: i32 {ada.type = @standard.integer} loc("A"(
-- MLIR-SAME:    %arg1: i32 {ada.type = @standard.integer} loc("B"(
-- MLIR-SAME:    %arg2: i1 {ada.type = @standard.boolean} loc("C"(

-- LLVM: DILocalVariable(name: "C", arg: 3,

function Param_NameLoc (A, B : Integer; C : Boolean) return Integer is
begin
   return A + B;
end Param_NameLoc;
