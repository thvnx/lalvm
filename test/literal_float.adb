-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.func @test_float_literal
-- MLIR-SAME:    () -> f32
-- MLIR:         %{{.*}} = arith.constant 1.000000e+00 : f32
-- MLIR-NEXT:    ada.return %{{.*}} : f32

-- LLVM-LABEL: define float @test_float_literal(
-- LLVM:         ret float

function Test_Float_Literal return Float is
begin
   return 1.0;
end Test_Float_Literal;
