-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.subp @test_div
-- MLIR:         %[[R:.*]] = ada.binop "/" %arg0, %arg1 {ada.type = @standard.integer} : i32
-- MLIR-NEXT:    ada.return %[[R]] : i32

-- LLVM-LABEL: define i32 @_ada_test_div(
-- LLVM:         %{{.*}} = sdiv i32 %0, %1
-- LLVM:         ret i32

function Test_Div (A, B : Integer) return Integer is
begin
   return A / B;
end Test_Div;
