-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.subp @test_constant_decl
-- MLIR-SAME:    () -> !ada.qual<i32, @standard.integer>
-- MLIR:         %[[X:.*]] = ada.constant : !ada.qual<i32, @standard.integer> = 5
-- MLIR-NEXT:    ada.return %[[X]] : !ada.qual<i32, @standard.integer>

-- LLVM-LABEL: define i32 @_ada_test_constant_decl(
-- LLVM:         ret i32 5

function Test_Constant_Decl return Integer is
   X : constant Integer := 5;
begin
   return X;
end Test_Constant_Decl;
