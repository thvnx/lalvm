-- RUN: %lalvm -O1 --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm -O1 --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.subp @local_var
-- MLIR-SAME:    () -> !ada.qual<i32, @standard.integer>
-- MLIR:         %[[X:.*]] = ada.constant : !ada.qual<i32, @standard.integer> = 5
-- MLIR-NEXT:    %[[Y:.*]] = ada.constant : !ada.qual<i32, @standard.integer> = 3
-- MLIR-NEXT:    %[[R:.*]] = ada.binop "+" %[[X]], %[[Y]] checks<overflow> : !ada.qual<i32, @standard.integer>
-- MLIR-NEXT:    ada.return %[[R]] : !ada.qual<i32, @standard.integer>

-- LLVM-LABEL: define i32 @_ada_local_var(
-- LLVM:         ret i32 8

function Local_Var return Integer is
   X : Integer := 5;
   Y : Integer := 3;
begin
   return X + Y;
end Local_Var;
