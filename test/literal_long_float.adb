-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.func @test_long_float_literal
-- MLIR-SAME:    () -> f64
-- MLIR:         %{{.*}} = arith.constant 1.000000e+00 : f64
-- MLIR-NEXT:    ada.return %{{.*}} : f64

-- LLVM-LABEL: define double @test_long_float_literal(
-- LLVM:         ret double

function Test_Long_Float_Literal return Long_Float is
begin
   return 1.0;
end Test_Long_Float_Literal;
