-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.subp @test_local_var_init_from_param
-- MLIR-SAME:    (%arg0: i32 {ada.type = @standard.integer}) -> i32
-- MLIR:         %[[PTR:.*]] = memref.alloca() : memref<i32>
-- MLIR-NEXT:    memref.store %arg0, %[[PTR]][] : memref<i32>
-- MLIR:         %[[X:.*]] = memref.load %[[PTR]][] : memref<i32>
-- MLIR:         ada.return %[[X]] : i32

-- LLVM-LABEL: define i32 @_ada_test_local_var_init_from_param(
-- LLVM:         ret i32 %0

function Test_Local_Var_Init_From_Param (N : Integer) return Integer is
   X : Integer := N;
begin
   return X;
end Test_Local_Var_Init_From_Param;
