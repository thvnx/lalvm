-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.subp @test_long_literal
-- MLIR-SAME:    () -> i64
-- MLIR:         %{{.*}} = arith.constant 42 : i64
-- MLIR-NEXT:    ada.return %{{.*}} : i64

-- LLVM-LABEL: define i64 @_ada_test_long_literal(
-- LLVM:         ret i64 42

function Test_Long_Literal return Long_Integer is
begin
   return 42;
end Test_Long_Literal;
