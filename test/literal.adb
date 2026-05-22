-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.subp @test_literal
-- MLIR:         %{{.*}} = arith.constant {ada.type = @standard.integer} 42 : i32
-- MLIR-NEXT:    ada.return %{{.*}} : i32

-- LLVM-LABEL: define i32 @_ada_test_literal(
-- LLVM:         ret i32 42

function Test_Literal return Integer is
begin
   return 42;
end Test_Literal;
