-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- User-defined "*" operator nested inside a procedure.
-- Ada operator syntax quotes ("*") are stripped by getName so the MLIR symbol
-- name is the bare operator symbol; MLIR then quotes it as @"*".

-- MLIR-LABEL: ada.subp @test_operator
-- MLIR:         ada.subp @"*"(%{{.*}}: !ada.qual<i32, @standard.integer>, %{{.*}}: !ada.qual<i32, @standard.integer>) -> !ada.qual<i32, @standard.integer>
-- MLIR:           %[[SUM:.*]] = ada.binop "+" %{{.*}}, %{{.*}} : !ada.qual<i32, @standard.integer>
-- MLIR-NEXT:      ada.return %[[SUM]] : !ada.qual<i32, @standard.integer>
-- MLIR:         ada.binop "*" %{{.*}}, %{{.*}} : !ada.qual<i32, @standard.integer>
-- MLIR:         ada.null
-- MLIR-NEXT:    ada.return

-- LLVM-LABEL: define void @_ada_test_operator(
-- LLVM:          ret void
-- LLVM-LABEL: define i32 @test_operator__Omultiply(
-- LLVM:          add i32
-- LLVM:          ret i32

procedure Test_Operator is

   function "*" (A : Integer; B : Integer) return Integer is
   begin
      return A + B;
   end "*";

   X, Y : Integer := 1;
   Z : Integer := X * Y;

begin
   null;
end;
