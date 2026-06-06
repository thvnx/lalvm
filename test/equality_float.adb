-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- Equality operator '=' on a floating-point type (RM 4.5.2). Lowered to
-- arith.cmpf with the ordered-equal predicate -> fcmp oeq, which is false when
-- either operand is NaN (1.0 = NaN and NaN = NaN are both False).

-- MLIR-LABEL: ada.subp @equality_float
-- MLIR:         ada.cmp "=" %arg0, %arg1 : (!ada.qual<f32, @standard.float>, !ada.qual<f32, @standard.float>) -> !ada.qual<i1, @standard.boolean>
-- MLIR:         %[[C:.*]] = ada.constant : !ada.qual<f32, @standard.float> = 1.000000e+00
-- MLIR:         ada.cmp "=" %arg0, %[[C]] : (!ada.qual<f32, @standard.float>, !ada.qual<f32, @standard.float>) -> !ada.qual<i1, @standard.boolean>

-- LLVM-LABEL: define i1 @_ada_equality_float(
-- LLVM-DAG:      fcmp oeq float %0, %1
-- LLVM-DAG:      fcmp oeq float %0, 1.000000e+00
-- LLVM:          ret i1

function Equality_Float (X, Y : Float) return Boolean is
   B1 : Boolean := X = Y;
   B2 : Boolean := X = 1.0;
begin
   return B1;
end Equality_Float;
