-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- Ordering operators '<', '<=', '>', '>=' on a floating-point type (RM 4.5.2):
-- operands share a type and the result is Boolean. Lowered to arith.cmpf with
-- the ordered predicates -> fcmp olt/ole/ogt/oge, which are false when either
-- operand is NaN. Distinct operands keep the comparisons from folding.

-- MLIR-LABEL: ada.subp @ordering_float
-- MLIR:         ada.cmp "<" %arg0, %arg1 : (!ada.qual<f32, @standard.float>, !ada.qual<f32, @standard.float>) -> !ada.qual<i1, @standard.boolean>
-- MLIR:         ada.cmp "<=" %arg0, %arg1 : (!ada.qual<f32, @standard.float>, !ada.qual<f32, @standard.float>) -> !ada.qual<i1, @standard.boolean>
-- MLIR:         ada.cmp ">" %arg0, %arg1 : (!ada.qual<f32, @standard.float>, !ada.qual<f32, @standard.float>) -> !ada.qual<i1, @standard.boolean>
-- MLIR:         ada.cmp ">=" %arg0, %arg1 : (!ada.qual<f32, @standard.float>, !ada.qual<f32, @standard.float>) -> !ada.qual<i1, @standard.boolean>

-- LLVM-LABEL: define i1 @_ada_ordering_float(
-- LLVM-DAG:      fcmp olt float %0, %1
-- LLVM-DAG:      fcmp ole float %0, %1
-- LLVM-DAG:      fcmp ogt float %0, %1
-- LLVM-DAG:      fcmp oge float %0, %1
-- LLVM:          ret i1

function Ordering_Float (X, Y : Float) return Boolean is
   A : Boolean := X < Y;
   B : Boolean := X <= Y;
   C : Boolean := X > Y;
   D : Boolean := X >= Y;
begin
   return A;
end Ordering_Float;
