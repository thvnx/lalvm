-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- Each identifier gets its own evaluation of the init expression (RM 3.3.1).
-- MLIR-LABEL: ada.subp @local_var_multi_name
-- MLIR-SAME:    () -> !ada.qual<i32, @standard.integer>
-- MLIR:         %[[X:.*]] = ada.constant : !ada.qual<i32, @standard.integer> = 3
-- MLIR-NEXT:    %[[Y:.*]] = ada.constant : !ada.qual<i32, @standard.integer> = 3
-- MLIR-NEXT:    %[[Z:.*]] = ada.constant : !ada.qual<i32, @standard.integer> = 3
-- MLIR-NEXT:    %[[R1:.*]] = ada.binop "+" %[[X]], %[[Y]] : !ada.qual<i32, @standard.integer>
-- MLIR-NEXT:    %[[R2:.*]] = ada.binop "+" %[[R1]], %[[Z]] : !ada.qual<i32, @standard.integer>
-- MLIR-NEXT:    ada.return %[[R2]] : !ada.qual<i32, @standard.integer>

-- LLVM-LABEL: define i32 @_ada_local_var_multi_name(
-- LLVM:         ret i32 9

function Local_Var_Multi_Name return Integer is
   X, Y, Z : Integer := 3;
begin
   return X + Y + Z;
end Local_Var_Multi_Name;
