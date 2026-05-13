-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.func @test_literal_arithmetic
-- MLIR-SAME:    () -> i32
-- MLIR:         %[[C1:.*]] = arith.constant 1 : i32
-- MLIR-NEXT:    %[[C2:.*]] = arith.constant 2 : i32
-- MLIR-NEXT:    %[[V0:.*]] = ada.add %[[C1]], %[[C2]] : i32
-- MLIR-NEXT:    %[[C3:.*]] = arith.constant 3 : i32
-- MLIR-NEXT:    %[[V1:.*]] = ada.add %[[V0]], %[[C3]] : i32
-- MLIR-NEXT:    %[[C4:.*]] = arith.constant 4 : i32
-- MLIR-NEXT:    %[[V2:.*]] = ada.add %[[V1]], %[[C4]] : i32
-- MLIR-NEXT:    %[[C5:.*]] = arith.constant 5 : i32
-- MLIR-NEXT:    %[[V3:.*]] = ada.add %[[V2]], %[[C5]] : i32
-- MLIR-NEXT:    %[[C6:.*]] = arith.constant 6 : i32
-- MLIR-NEXT:    %[[V4:.*]] = ada.sub %[[V3]], %[[C6]] : i32
-- MLIR-NEXT:    %[[C7:.*]] = arith.constant 7 : i32
-- MLIR-NEXT:    %[[C8:.*]] = arith.constant 8 : i32
-- MLIR-NEXT:    %[[V5:.*]] = ada.mul %[[C7]], %[[C8]] : i32
-- MLIR-NEXT:    %[[V6:.*]] = ada.add %[[V4]], %[[V5]] : i32
-- MLIR-NEXT:    %[[C9:.*]] = arith.constant 9 : i32
-- MLIR-NEXT:    %[[V7:.*]] = ada.sub %[[V6]], %[[C9]] : i32
-- MLIR-NEXT:    %[[C10:.*]] = arith.constant 10 : i32
-- MLIR-NEXT:    %[[V8:.*]] = ada.add %[[V7]], %[[C10]] : i32
-- MLIR-NEXT:    ada.return %[[V8]] : i32

-- LLVM-LABEL: define i32 @_ada_test_literal_arithmetic(
-- LLVM:         ret i32 66

-- 1 + 2 + 3 + 4 + 5 - 6 + 7 * 8 - 9 + 10 = 66
function Test_Literal_Arithmetic return Integer is
begin
   return 1 + 2 + 3 + 4 + 5 - 6 + 7 * 8 - 9 + 10;
end Test_Literal_Arithmetic;
