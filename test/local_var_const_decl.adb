-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.subp @test_constant_decl
-- MLIR-SAME:    () -> i32
-- MLIR:         %[[INIT:.*]] = arith.constant 5 : i32
-- MLIR-NEXT:    %[[PTR:.*]] = memref.alloca() : memref<i32>
-- MLIR-NEXT:    memref.store %[[INIT]], %[[PTR]][] : memref<i32>
-- MLIR:         %[[X:.*]] = memref.load %[[PTR]][] : memref<i32>
-- MLIR:         ada.return %[[X]] : i32

-- LLVM-LABEL: define i32 @_ada_test_constant_decl(
-- LLVM:         #dbg_value(
-- LLVM:         ret i32 5

function Test_Constant_Decl return Integer is
   X : constant Integer := 5;
begin
   return X;
end Test_Constant_Decl;
