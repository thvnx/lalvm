-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.subp @long_integer
-- MLIR-SAME:    (%arg0: !ada.qual<i64, @standard.long_integer>, %arg1: !ada.qual<i64, @standard.long_integer>) -> !ada.qual<i64, @standard.long_integer>
-- MLIR:         %[[R:.*]] = ada.binop "+" %arg0, %arg1 checks<overflow> : !ada.qual<i64, @standard.long_integer>
-- MLIR-NEXT:    ada.return %[[R]] : !ada.qual<i64, @standard.long_integer>

-- LLVM-LABEL: define i64 @_ada_long_integer(
-- LLVM:         %{{.*}} = add i64 %0, %1
-- LLVM:         ret i64

function Long_Integer (A, B : Long_Integer) return Long_Integer is
begin
   return A + B;
end Long_Integer;
