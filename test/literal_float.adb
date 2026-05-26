-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.subp @test_float_literal
-- MLIR-SAME:    () -> !ada.qual<f32, @standard.float>
-- MLIR:         %{{.*}} = ada.constant : !ada.qual<f32, @standard.float> = 1.000000e+00
-- MLIR-NEXT:    ada.return %{{.*}} : !ada.qual<f32, @standard.float>

-- LLVM-LABEL: define float @_ada_test_float_literal(
-- LLVM:         ret float

function Test_Float_Literal return Float is
begin
   return 1.0;
end Test_Float_Literal;
