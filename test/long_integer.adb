-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.subp @test_long
-- MLIR-SAME:    (%arg0: i64 {ada.type = @standard.long_integer}, %arg1: i64 {ada.type = @standard.long_integer}) -> i64
-- MLIR:         %[[R:.*]] = ada.binop "+" %arg0, %arg1 {ada.type = @standard.long_integer} : i64
-- MLIR-NEXT:    ada.return %[[R]] : i64

-- LLVM-LABEL: define i64 @_ada_test_long(
-- LLVM:         %{{.*}} = add i64 %0, %1
-- LLVM:         ret i64

function Test_Long (A, B : Long_Integer) return Long_Integer is
begin
   return A + B;
end Test_Long;
