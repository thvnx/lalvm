-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.subp @local_var_const_decl
-- MLIR-SAME:    () -> !ada.qual<i32, @standard.integer>
-- MLIR:         %[[X:.*]] = ada.constant : !ada.qual<i32, @standard.integer> = 5
-- MLIR-NEXT:    ada.return %[[X]] : !ada.qual<i32, @standard.integer>

-- LLVM-LABEL: define i32 @_ada_local_var_const_decl(
-- LLVM:         ret i32 5

function Local_Var_Const_Decl return Integer is
   X : constant Integer := 5;
begin
   return X;
end Local_Var_Const_Decl;
