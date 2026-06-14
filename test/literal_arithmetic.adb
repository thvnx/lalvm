-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- The whole expression is static (RM 4.9), so it is evaluated at compile time
-- to a single constant: no arithmetic ops and no overflow check are emitted.
-- MLIR-LABEL: ada.subp @literal_arithmetic
-- MLIR-SAME:    () -> !ada.qual<i32, @standard.integer>
-- MLIR:         %[[V:.*]] = ada.constant : !ada.qual<i32, @standard.integer> = 66
-- MLIR-NEXT:    ada.return %[[V]] : !ada.qual<i32, @standard.integer>

-- LLVM-LABEL: define i32 @_ada_literal_arithmetic(
-- LLVM:         ret i32 66

-- 1 + 2 + 3 + 4 + 5 - 6 + 7 * 8 - 9 + 10 = 66
function Literal_Arithmetic return Integer is
begin
   return 1 + 2 + 3 + 4 + 5 - 6 + 7 * 8 - 9 + 10;
end Literal_Arithmetic;
