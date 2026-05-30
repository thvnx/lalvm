-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.subp @literal_long_float
-- MLIR-SAME:    () -> !ada.qual<f64, @standard.long_float>
-- MLIR:         %{{.*}} = ada.constant : !ada.qual<f64, @standard.long_float> = 1.000000e+00
-- MLIR-NEXT:    ada.return %{{.*}} : !ada.qual<f64, @standard.long_float>

-- LLVM-LABEL: define double @_ada_literal_long_float(
-- LLVM:         ret double

function Literal_Long_Float return Long_Float is
begin
   return 1.0;
end Literal_Long_Float;
