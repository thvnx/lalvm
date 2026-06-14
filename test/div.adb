-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.subp @div
-- MLIR:         %[[R:.*]] = ada.binop "/" %arg0, %arg1 checks<division> : !ada.qual<i32, @standard.integer>
-- MLIR-NEXT:    ada.return %[[R]] : !ada.qual<i32, @standard.integer>

-- LLVM-LABEL: define i32 @_ada_div(
-- LLVM:         %{{.*}} = sdiv i32 %0, %1
-- LLVM:         ret i32

function Div (A, B : Integer) return Integer is
begin
   return A / B;
end Div;
