-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.func @test_constant_decl
-- MLIR-SAME:    () -> i32
-- MLIR:         %[[X:.*]] = arith.constant 5 : i32
-- MLIR-NEXT:    ada.return %[[X]] : i32

-- LLVM-LABEL: define i32 @test_constant_decl(
-- LLVM:         ret i32 5

function Test_Constant_Decl return Integer is
   X : constant Integer := 5;
begin
   return X;
end Test_Constant_Decl;
