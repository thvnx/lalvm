-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.subp @test
-- MLIR:         ada.subp @inner(
-- MLIR:           ada.return %{{.*}} : i32
-- MLIR:         %[[C1:.*]] = arith.constant 42 : i32
-- MLIR:         %[[V1:.*]] = ada.call @inner(%[[C1]]) : (i32) -> i32
-- MLIR:         %[[C2:.*]] = arith.constant 42 : i32
-- MLIR:         %[[V2:.*]] = ada.call @inner(%[[C2]]) : (i32) -> i32
-- MLIR:         ada.return

-- LLVM-LABEL: define void @_ada_test(
-- LLVM:          call i32 @test__inner(i32 42)
-- LLVM:          call i32 @test__inner(i32 42)
-- LLVM:          ret void
-- LLVM-LABEL: define i32 @test__inner(i32
-- LLVM:          ret i32 %0

procedure Test is
   function Inner (X : Integer) return Integer;

   function Inner (X : Integer) return Integer is
      I : Integer := X;
   begin
      return I;
   end Inner;

   I : Integer := Inner (42);
begin
   I := Inner (42);
end Test;
