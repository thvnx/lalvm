-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.func @test_sub
-- MLIR:         %[[R:.*]] = ada.sub %arg0, %arg1 : i32
-- MLIR-NEXT:    ada.return %[[R]] : i32

-- LLVM-LABEL: define i32 @test_sub(
-- LLVM:         %{{.*}} = sub i32 %0, %1
-- LLVM:         ret i32

function Test_Sub (A, B : Integer) return Integer is
begin
   return A - B;
end Test_Sub;
