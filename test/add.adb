-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.func @test
-- MLIR:         %[[AB:.*]] = ada.binop "+" %arg0, %arg1 : i32
-- MLIR-NEXT:    %[[ABC:.*]] = ada.binop "+" %[[AB]], %arg2 : i32
-- MLIR-NEXT:    ada.return %[[ABC]] : i32

-- LLVM-LABEL: define i32 @_ada_test(
-- LLVM:         %{{.*}} = add i32 %0, %1
-- LLVM-NEXT:    %{{.*}} = add i32 %{{.*}}, %2
-- LLVM:         ret i32

function Test (I, J, K : Integer) return Integer is
begin
   return I + J + K;
end Test;
