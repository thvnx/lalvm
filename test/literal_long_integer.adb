-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.subp @literal_long_integer
-- MLIR-SAME:    () -> !ada.qual<i64, @standard.long_integer>
-- MLIR:         %{{.*}} = ada.constant : !ada.qual<i64, @standard.long_integer> = 42
-- MLIR-NEXT:    ada.return %{{.*}} : !ada.qual<i64, @standard.long_integer>

-- LLVM-LABEL: define i64 @_ada_literal_long_integer(
-- LLVM:         ret i64 42

function Literal_Long_Integer return Long_Integer is
begin
   return 42;
end Literal_Long_Integer;
