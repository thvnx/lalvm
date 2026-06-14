-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- User-defined "*" operator nested inside a procedure.
-- Ada operator syntax quotes ("*") are stripped by getName so the MLIR symbol
-- name is the bare operator symbol; MLIR then quotes it as @"operator_overload.*".

-- MLIR-LABEL: ada.subp @operator_overload
-- MLIR:         ada.binop "*" %{{.*}}, %{{.*}} checks<overflow> : !ada.qual<i32, @standard.integer>
-- MLIR:         ada.decls {
-- MLIR:           ada.subp private @"operator_overload.*"(%{{.*}}: !ada.qual<i32, @standard.integer>, %{{.*}}: !ada.qual<i32, @standard.integer>) -> !ada.qual<i32, @standard.integer>
-- MLIR:             %[[SUM:.*]] = ada.binop "+" %{{.*}}, %{{.*}} checks<overflow> : !ada.qual<i32, @standard.integer>
-- MLIR-NEXT:        ada.return %[[SUM]] : !ada.qual<i32, @standard.integer>
-- MLIR:         ada.null
-- MLIR-NEXT:    ada.return

-- LLVM-LABEL: define void @_ada_operator_overload(
-- LLVM:          ret void
-- LLVM-LABEL: define internal i32 @operator_overload__Omultiply(
-- LLVM:          add i32
-- LLVM:          ret i32

procedure Operator_Overload is

   function "*" (A : Integer; B : Integer) return Integer is
   begin
      return A + B;
   end "*";

   X, Y : Integer := 1;
   Z : Integer := X * Y;

begin
   null;
end;
