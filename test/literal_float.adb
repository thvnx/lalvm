-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.subp @literal_float
-- MLIR-SAME:    () -> !ada.qual<f32, @standard.float>
-- MLIR:         %{{.*}} = ada.constant : !ada.qual<f32, @standard.float> = 1.000000e+00
-- MLIR-NEXT:    ada.return %{{.*}} : !ada.qual<f32, @standard.float>

-- LLVM-LABEL: define float @_ada_literal_float(
-- LLVM:         ret float

function Literal_Float return Float is
begin
   return 1.0;
end Literal_Float;
