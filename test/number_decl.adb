-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- Named numbers (RM 3.3.2): an arith.constant is emitted at declaration for
-- DWARF debug info (universal type). At each use site, the value is re-emitted
-- in the concrete MLIR type resolved from the use context.

-- MLIR: ada.type @standard.universal_real_type_ : f64 = #ada.numeric_info
-- MLIR: ada.type @standard.universal_int_type_ : i64 = #ada.numeric_info
-- MLIR-LABEL: ada.subp @number_decl()
-- MLIR:         ada.subp @f() -> !ada.qual<i32, @standard.integer>
-- MLIR:           %[[UMAX:.*]] = ada.constant : !ada.qual<i64, @standard.universal_int_type_> = 200
-- MLIR-NEXT:      %[[MAX:.*]] = ada.coerce %[[UMAX]] : <i64, @standard.universal_int_type_> to <i32, @standard.integer>
-- MLIR-NEXT:      ada.return %[[MAX]] : !ada.qual<i32, @standard.integer>
-- MLIR:         ada.subp @g() -> !ada.qual<f32, @standard.float>
-- MLIR:           %[[UPI:.*]] = ada.constant : !ada.qual<f64, @standard.universal_real_type_> = {{.*}}
-- MLIR-NEXT:      %[[PI:.*]] = ada.coerce %[[UPI]] : <f64, @standard.universal_real_type_> to <f32, @standard.float>
-- MLIR-NEXT:      ada.return %[[PI]] : !ada.qual<f32, @standard.float>

-- LLVM-LABEL: define void @_ada_number_decl(
-- LLVM-LABEL: define i32 @number_decl__f(
-- LLVM:          ret i32 200
-- LLVM-LABEL: define float @number_decl__g(
-- LLVM:          ret float

procedure Number_Decl is
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
end Number_Decl;
