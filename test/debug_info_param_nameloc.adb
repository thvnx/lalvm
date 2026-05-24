-- RUN: %lalvm --emit=mlir --mlir-print-debuginfo --mlir-print-local-scope %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR: ada.type @standard.boolean : i1 = #ada.enum_info<"false" = 0, "true" = 1>
-- MLIR-LABEL: ada.subp @param_nameloc(
-- MLIR-SAME:    %arg0: i32 loc("a"(fused<@standard.integer>[
-- MLIR-SAME:    %arg1: i32 loc("b"(fused<@standard.integer>[
-- MLIR-SAME:    %arg2: i1 loc("c"(fused<@standard.boolean>[

-- LLVM: DILocalVariable(name: "c", arg: 3,

function Param_NameLoc (A, B : Integer; C : Boolean) return Integer is
begin
   return A + B;
end Param_NameLoc;
