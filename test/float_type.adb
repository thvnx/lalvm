-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR: ada.type @standard.float : f32 = #ada.float_info<digits 6>
-- MLIR-LABEL: ada.subp @float_type
-- MLIR-SAME:    (%arg0: !ada.qual<f32, @standard.float>, %arg1: !ada.qual<f32, @standard.float>) -> !ada.qual<f32, @standard.float>
-- MLIR:         %[[R:.*]] = ada.binop "+" %arg0, %arg1 : !ada.qual<f32, @standard.float>
-- MLIR-NEXT:    ada.return %[[R]] : !ada.qual<f32, @standard.float>

-- LLVM-LABEL: define float @_ada_float_type(
-- LLVM:         %{{.*}} = fadd float %0, %1
-- LLVM:         ret float

function Float_Type (A, B : Float) return Float is
begin
   return A + B;
end Float_Type;
