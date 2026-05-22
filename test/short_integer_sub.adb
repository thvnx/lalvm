-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.subp @test_short_sub
-- MLIR-SAME:    (%arg0: i16 {ada.type = @standard.short_integer}, %arg1: i16 {ada.type = @standard.short_integer}) -> i16
-- MLIR:         %[[R:.*]] = ada.binop "-" %arg0, %arg1 {ada.type = @standard.short_integer} : i16
-- MLIR-NEXT:    ada.return %[[R]] : i16

-- LLVM-LABEL: define i16 @_ada_test_short_sub(
-- LLVM:         %{{.*}} = sub i16 %0, %1
-- LLVM:         ret i16

function Test_Short_Sub (A, B : Short_Integer) return Short_Integer is
begin
   return A - B;
end Test_Short_Sub;
