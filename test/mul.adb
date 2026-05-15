-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.func @test_mul
-- MLIR:         %[[R:.*]] = ada.binop "*" %arg0, %arg1 : i32
-- MLIR-NEXT:    ada.return %[[R]] : i32

-- LLVM-LABEL: define i32 @_ada_test_mul(
-- LLVM:         %{{.*}} = mul i32 %0, %1
-- LLVM:         ret i32

function Test_Mul (A, B : Integer) return Integer is
begin
   return A * B;
end Test_Mul;
