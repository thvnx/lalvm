-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.subp @test_sub
-- MLIR:         %[[R:.*]] = ada.binop "-" %arg0, %arg1 : !ada.qual<i32, @standard.integer>
-- MLIR-NEXT:    ada.return %[[R]] : !ada.qual<i32, @standard.integer>

-- LLVM-LABEL: define i32 @_ada_test_sub(
-- LLVM:         %{{.*}} = sub i32 %0, %1
-- LLVM:         ret i32

function Test_Sub (A, B : Integer) return Integer is
begin
   return A - B;
end Test_Sub;
