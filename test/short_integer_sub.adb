-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.subp @test_short_sub
-- MLIR-SAME:    (%arg0: !ada.qual<i16, @standard.short_integer>, %arg1: !ada.qual<i16, @standard.short_integer>) -> !ada.qual<i16, @standard.short_integer>
-- MLIR:         %[[R:.*]] = ada.binop "-" %arg0, %arg1 : !ada.qual<i16, @standard.short_integer>
-- MLIR-NEXT:    ada.return %[[R]] : !ada.qual<i16, @standard.short_integer>

-- LLVM-LABEL: define i16 @_ada_test_short_sub(
-- LLVM:         %{{.*}} = sub i16 %0, %1
-- LLVM:         ret i16

function Test_Short_Sub (A, B : Short_Integer) return Short_Integer is
begin
   return A - B;
end Test_Short_Sub;
