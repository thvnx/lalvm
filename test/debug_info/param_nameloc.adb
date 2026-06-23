-- RUN: %lalvm --emit=mlir --mlir-print-debuginfo --mlir-print-local-scope %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR: ada.type @standard.boolean : i1 = #ada.enum_info<"false" = 0, "true" = 1>
-- MLIR-LABEL: ada.subp @param_nameloc(
-- MLIR-SAME:    %arg0: !ada.qual<i32, @standard.integer> loc("a"(
-- MLIR-SAME:    %arg1: !ada.qual<i32, @standard.integer> loc("b"(
-- MLIR-SAME:    %arg2: !ada.qual<i1, @standard.boolean> loc("c"(

-- LLVM: DILocalVariable(name: "c", arg: 3,

function Param_Nameloc (A, B : Integer; C : Boolean) return Integer is
begin
   return A + B;
end Param_Nameloc;
