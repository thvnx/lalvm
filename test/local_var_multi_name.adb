-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- Each identifier gets its own evaluation of the init expression (RM 3.3.1).
-- MLIR-LABEL: ada.subp @test_local_var_multi_name
-- MLIR-SAME:    () -> i32
-- MLIR:         %[[X_INIT:.*]] = arith.constant 3 : i32
-- MLIR-NEXT:    %[[X_PTR:.*]] = memref.alloca(){{.*}}: memref<i32>
-- MLIR-NEXT:    memref.store %[[X_INIT]], %[[X_PTR]][] : memref<i32>
-- MLIR-NEXT:    %[[Y_INIT:.*]] = arith.constant 3 : i32
-- MLIR-NEXT:    %[[Y_PTR:.*]] = memref.alloca(){{.*}}: memref<i32>
-- MLIR-NEXT:    memref.store %[[Y_INIT]], %[[Y_PTR]][] : memref<i32>
-- MLIR-NEXT:    %[[Z_INIT:.*]] = arith.constant 3 : i32
-- MLIR-NEXT:    %[[Z_PTR:.*]] = memref.alloca(){{.*}}: memref<i32>
-- MLIR-NEXT:    memref.store %[[Z_INIT]], %[[Z_PTR]][] : memref<i32>
-- MLIR:         %[[X:.*]] = memref.load %[[X_PTR]][] : memref<i32>
-- MLIR:         %[[Y:.*]] = memref.load %[[Y_PTR]][] : memref<i32>
-- MLIR:         %[[R1:.*]] = ada.binop "+" %[[X]], %[[Y]] : i32
-- MLIR:         %[[Z:.*]] = memref.load %[[Z_PTR]][] : memref<i32>
-- MLIR:         %[[R2:.*]] = ada.binop "+" %[[R1]], %[[Z]] : i32
-- MLIR:         ada.return %[[R2]] : i32

-- LLVM-LABEL: define i32 @_ada_test_local_var_multi_name(
-- LLVM:         ret i32 9

function Test_Local_Var_Multi_Name return Integer is
   X, Y, Z : Integer := 3;
begin
   return X + Y + Z;
end Test_Local_Var_Multi_Name;
