-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- Float division (RM 4.5.5) lowers to `fdiv`; unlike integer `/` it carries
-- no division check (no zero-divisor raise for reals).

-- MLIR-LABEL: ada.subp @long_float_div
-- MLIR:         %[[R:.*]] = ada.binop "/" %arg0, %arg1 : !ada.qual<f64, @standard.long_float>
-- MLIR-NEXT:    ada.return %[[R]] : !ada.qual<f64, @standard.long_float>

-- LLVM-LABEL: define double @_ada_long_float_div(
-- LLVM:         %{{.*}} = fdiv double %0, %1
-- LLVM:         ret double

function Long_Float_Div (A, B : Long_Float) return Long_Float is
begin
   return A / B;
end Long_Float_Div;
