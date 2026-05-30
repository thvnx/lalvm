-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.subp @long_float_sub
-- MLIR-SAME:    (%arg0: !ada.qual<f64, @standard.long_float>, %arg1: !ada.qual<f64, @standard.long_float>) -> !ada.qual<f64, @standard.long_float>
-- MLIR:         %[[R:.*]] = ada.binop "-" %arg0, %arg1 : !ada.qual<f64, @standard.long_float>
-- MLIR-NEXT:    ada.return %[[R]] : !ada.qual<f64, @standard.long_float>

-- LLVM-LABEL: define double @_ada_long_float_sub(
-- LLVM:         %{{.*}} = fsub double %0, %1
-- LLVM:         ret double

function Long_Float_Sub (A, B : Long_Float) return Long_Float is
begin
   return A - B;
end Long_Float_Sub;
