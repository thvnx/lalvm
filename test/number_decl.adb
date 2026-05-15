-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- Named numbers (RM 3.3.2): the constant is emitted at each use site with
-- the concrete MLIR type resolved from the use context.

-- MLIR-LABEL: ada.proc @test_number_decl()
-- MLIR:         ada.func @f() -> i32
-- MLIR:           %[[MAX:.*]] = arith.constant 200 : i32
-- MLIR:           ada.return %[[MAX]] : i32
-- MLIR:         ada.func @g() -> f32
-- MLIR:           %[[PI:.*]] = arith.constant 3.141590e+00 : f32
-- MLIR:           ada.return %[[PI]] : f32

-- LLVM-LABEL: define void @_ada_test_number_decl(
-- LLVM-LABEL: define i32 @test_number_decl__f(
-- LLVM:          ret i32 200
-- LLVM-LABEL: define float @test_number_decl__g(
-- LLVM:          ret float

procedure Test_Number_Decl is
   function F return Integer is
      Max : constant := 200;
   begin
      return Max;
   end F;

   function G return Float is
      Pi : constant := 3.14159;
   begin
      return Pi;
   end G;
begin
   null;
end Test_Number_Decl;
