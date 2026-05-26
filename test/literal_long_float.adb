-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.subp @test_long_float_literal
-- MLIR-SAME:    () -> !ada.qual<f64, @standard.long_float>
-- MLIR:         %{{.*}} = ada.constant : !ada.qual<f64, @standard.long_float> = 1.000000e+00
-- MLIR-NEXT:    ada.return %{{.*}} : !ada.qual<f64, @standard.long_float>

-- LLVM-LABEL: define double @_ada_test_long_float_literal(
-- LLVM:         ret double

function Test_Long_Float_Literal return Long_Float is
begin
   return 1.0;
end Test_Long_Float_Literal;
