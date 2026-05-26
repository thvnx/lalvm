-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- Each identifier gets its own evaluation of the init expression (RM 3.3.1).
-- MLIR-LABEL: ada.subp @test_local_var_multi_name
-- MLIR-SAME:    () -> i32
-- MLIR:         %[[X:.*]] = arith.constant 3 : i32
-- MLIR-NEXT:    %[[Y:.*]] = arith.constant 3 : i32
-- MLIR-NEXT:    %[[Z:.*]] = arith.constant 3 : i32
-- MLIR-NEXT:    %[[R1:.*]] = ada.binop "+" %[[X]], %[[Y]] : i32
-- MLIR-NEXT:    %[[R2:.*]] = ada.binop "+" %[[R1]], %[[Z]] : i32
-- MLIR-NEXT:    ada.return %[[R2]] : i32

-- LLVM-LABEL: define i32 @_ada_test_local_var_multi_name(
-- LLVM:         ret i32 9

function Test_Local_Var_Multi_Name return Integer is
   X, Y, Z : Integer := 3;
begin
   return X + Y + Z;
end Test_Local_Var_Multi_Name;
