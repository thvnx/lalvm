-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.subp @test_float
-- MLIR-SAME:    (%arg0: !ada.qual<f32, @standard.float>, %arg1: !ada.qual<f32, @standard.float>) -> !ada.qual<f32, @standard.float>
-- MLIR:         %[[R:.*]] = ada.binop "+" %arg0, %arg1 : !ada.qual<f32, @standard.float>
-- MLIR-NEXT:    ada.return %[[R]] : !ada.qual<f32, @standard.float>

-- LLVM-LABEL: define float @_ada_test_float(
-- LLVM:         %{{.*}} = fadd float %0, %1
-- LLVM:         ret float

function Test_Float (A, B : Float) return Float is
begin
   return A + B;
end Test_Float;
