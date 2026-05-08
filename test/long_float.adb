-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.func @test_long_float
-- MLIR-SAME:    (%arg0: f64, %arg1: f64) -> f64
-- MLIR:         %[[R:.*]] = ada.add %arg0, %arg1 : f64
-- MLIR-NEXT:    ada.return %[[R]] : f64

-- LLVM-LABEL: define double @test_long_float(
-- LLVM:         %{{.*}} = fadd double %0, %1
-- LLVM:         ret double

function Test_Long_Float (A, B : Long_Float) return Long_Float is
begin
   return A + B;
end Test_Long_Float;
