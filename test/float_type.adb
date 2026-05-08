-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.func @test_float
-- MLIR-SAME:    (%arg0: f32, %arg1: f32) -> f32
-- MLIR:         %[[R:.*]] = ada.add %arg0, %arg1 : f32
-- MLIR-NEXT:    ada.return %[[R]] : f32

-- LLVM-LABEL: define float @test_float(
-- LLVM:         %{{.*}} = fadd float %0, %1
-- LLVM:         ret float

function Test_Float (A, B : Float) return Float is
begin
   return A + B;
end Test_Float;
