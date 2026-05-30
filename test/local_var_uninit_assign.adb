-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.subp @local_var_uninit_assign
-- MLIR-SAME:    () -> !ada.qual<i32, @standard.integer>
-- MLIR:         %[[X:.*]] = ada.constant : !ada.qual<i32, @standard.integer> = 42
-- MLIR-NEXT:    ada.return %[[X]] : !ada.qual<i32, @standard.integer>

-- LLVM-LABEL: define i32 @_ada_local_var_uninit_assign(
-- LLVM:          ret i32 42

function Local_Var_Uninit_Assign return Integer is
   X : Integer;
begin
   X := 42;
   return X;
end Local_Var_Uninit_Assign;
