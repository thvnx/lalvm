-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- Named numbers (RM 3.3.2): an arith.constant is emitted at declaration for
-- DWARF debug info (universal type). At each use site, the value is re-emitted
-- in the concrete MLIR type resolved from the use context.

-- MLIR: ada.type @standard.universal_real_type_ : f64 = #ada.numeric_info
-- MLIR: ada.type @standard.universal_int_type_ : i64 = #ada.numeric_info
-- MLIR-LABEL: ada.subp @test_number_decl()
-- MLIR:         ada.subp @f() -> i32
-- MLIR:           arith.constant {ada.type = @standard.universal_int_type_} 200 : i64
-- MLIR:           %[[MAX:.*]] = arith.constant {ada.type = @standard.integer} 200 : i32
-- MLIR:           ada.return %[[MAX]] : i32
-- MLIR:         ada.subp @g() -> f32
-- MLIR:           arith.constant {ada.type = @standard.universal_real_type_} 3.141590e+00 : f64
-- MLIR:           %[[PI:.*]] = arith.constant {ada.type = @standard.float} 3.141590e+00 : f32
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
