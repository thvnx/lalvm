-- RUN: %lalvm -O1 --emit=llvm --mlir-print-ir-after=canonicalize %s -o /dev/null 2>&1 | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm -O1 --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: func.func @_ada_local_var_init_from_param
-- MLIR-SAME:    (%n: !ada.qual<i32, @standard.integer>) -> !ada.qual<i32, @standard.integer>
-- MLIR:         return %n : !ada.qual<i32, @standard.integer>

-- LLVM-LABEL: define i32 @_ada_local_var_init_from_param(
-- LLVM:         ret i32 %0

function Local_Var_Init_From_Param (N : Integer) return Integer is
   X : Integer := N;
begin
   return X;
end Local_Var_Init_From_Param;
