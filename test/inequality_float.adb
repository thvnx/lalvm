-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- Inequality operator '/=' on a floating-point type. A Boolean '/=' is the
-- complement of '=' (RM 6.6), so it must lower to the exact negation of the
-- ordered-equal predicate: arith.cmpf une -> fcmp une, which is true when
-- either operand is NaN (1.0 /= NaN and NaN /= NaN are both True). The 'one'
-- predicate would be wrong here (false for NaN), so this guards against it.

-- MLIR-LABEL: ada.subp @inequality_float
-- MLIR:         ada.cmp "/=" %arg0, %arg1 : (!ada.qual<f32, @standard.float>, !ada.qual<f32, @standard.float>) -> !ada.qual<i1, @standard.boolean>
-- MLIR:         %[[C:.*]] = ada.constant : !ada.qual<f32, @standard.float> = 1.000000e+00
-- MLIR:         ada.cmp "/=" %arg0, %[[C]] : (!ada.qual<f32, @standard.float>, !ada.qual<f32, @standard.float>) -> !ada.qual<i1, @standard.boolean>

-- LLVM-LABEL: define i1 @_ada_inequality_float(
-- LLVM-DAG:      fcmp une float %0, %1
-- LLVM-DAG:      fcmp une float %0, 1.000000e+00
-- LLVM:          ret i1

function Inequality_Float (X, Y : Float) return Boolean is
   B1 : Boolean := X /= Y;
   B2 : Boolean := X /= 1.0;
begin
   return B1;
end Inequality_Float;
