-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.subp @short_integer_sub
-- MLIR-SAME:    (%arg0: !ada.qual<i16, @standard.short_integer>, %arg1: !ada.qual<i16, @standard.short_integer>) -> !ada.qual<i16, @standard.short_integer>
-- MLIR:         %[[R:.*]] = ada.binop "-" %arg0, %arg1 : !ada.qual<i16, @standard.short_integer>
-- MLIR-NEXT:    ada.return %[[R]] : !ada.qual<i16, @standard.short_integer>

-- LLVM-LABEL: define i16 @_ada_short_integer_sub(
-- LLVM:         %{{.*}} = sub i16 %0, %1
-- LLVM:         ret i16

function Short_Integer_Sub (A, B : Short_Integer) return Short_Integer is
begin
   return A - B;
end Short_Integer_Sub;
