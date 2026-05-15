-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.func @test_short
-- MLIR-SAME:    (%arg0: i16, %arg1: i16) -> i16
-- MLIR:         %[[R:.*]] = ada.binop "+" %arg0, %arg1 : i16
-- MLIR-NEXT:    ada.return %[[R]] : i16

-- LLVM-LABEL: define i16 @_ada_test_short(
-- LLVM:         %{{.*}} = add i16 %0, %1
-- LLVM:         ret i16

function Test_Short (A, B : Short_Integer) return Short_Integer is
begin
   return A + B;
end Test_Short;
