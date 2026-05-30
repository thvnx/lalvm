-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.subp @mul
-- MLIR:         %[[R:.*]] = ada.binop "*" %arg0, %arg1 : !ada.qual<i32, @standard.integer>
-- MLIR-NEXT:    ada.return %[[R]] : !ada.qual<i32, @standard.integer>

-- LLVM-LABEL: define i32 @_ada_mul(
-- LLVM:         %{{.*}} = mul i32 %0, %1
-- LLVM:         ret i32

function Mul (A, B : Integer) return Integer is
begin
   return A * B;
end Mul;
