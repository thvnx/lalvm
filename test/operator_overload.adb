-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- User-defined "*" operator nested inside a procedure.
-- @"\22*\22" is MLIR's encoding of the Ada name "*" (double-quote delimiters
-- are Ada operator notation, escaped as \22 in MLIR symbol names).

-- MLIR-LABEL: ada.subp @test_operator
-- MLIR:         ada.subp @"\22*\22"(%{{.*}}: i32 {ada.type = @standard.integer}, %{{.*}}: i32 {ada.type = @standard.integer}) -> i32
-- MLIR:           %[[SUM:.*]] = ada.binop "+" %{{.*}}, %{{.*}} {ada.type = @standard.integer} : i32
-- MLIR-NEXT:      ada.return %[[SUM]] : i32
-- MLIR:         ada.binop "*" %{{.*}}, %{{.*}} {ada.type = @standard.integer} : i32
-- MLIR:         ada.null
-- MLIR-NEXT:    ada.return

-- LLVM-LABEL: define void @_ada_test_operator(
-- LLVM:          ret void
-- LLVM-LABEL: define i32 @"test_operator__\22*\22"(
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
