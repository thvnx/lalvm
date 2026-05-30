-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.subp @literal_arithmetic
-- MLIR-SAME:    () -> !ada.qual<i32, @standard.integer>
-- MLIR:         %[[C1:.*]] = ada.constant : !ada.qual<i32, @standard.integer> = 1
-- MLIR-NEXT:    %[[C2:.*]] = ada.constant : !ada.qual<i32, @standard.integer> = 2
-- MLIR-NEXT:    %[[V0:.*]] = ada.binop "+" %[[C1]], %[[C2]] : !ada.qual<i32, @standard.integer>
-- MLIR-NEXT:    %[[C3:.*]] = ada.constant : !ada.qual<i32, @standard.integer> = 3
-- MLIR-NEXT:    %[[V1:.*]] = ada.binop "+" %[[V0]], %[[C3]] : !ada.qual<i32, @standard.integer>
-- MLIR-NEXT:    %[[C4:.*]] = ada.constant : !ada.qual<i32, @standard.integer> = 4
-- MLIR-NEXT:    %[[V2:.*]] = ada.binop "+" %[[V1]], %[[C4]] : !ada.qual<i32, @standard.integer>
-- MLIR-NEXT:    %[[C5:.*]] = ada.constant : !ada.qual<i32, @standard.integer> = 5
-- MLIR-NEXT:    %[[V3:.*]] = ada.binop "+" %[[V2]], %[[C5]] : !ada.qual<i32, @standard.integer>
-- MLIR-NEXT:    %[[C6:.*]] = ada.constant : !ada.qual<i32, @standard.integer> = 6
-- MLIR-NEXT:    %[[V4:.*]] = ada.binop "-" %[[V3]], %[[C6]] : !ada.qual<i32, @standard.integer>
-- MLIR-NEXT:    %[[C7:.*]] = ada.constant : !ada.qual<i32, @standard.integer> = 7
-- MLIR-NEXT:    %[[C8:.*]] = ada.constant : !ada.qual<i32, @standard.integer> = 8
-- MLIR-NEXT:    %[[V5:.*]] = ada.binop "*" %[[C7]], %[[C8]] : !ada.qual<i32, @standard.integer>
-- MLIR-NEXT:    %[[V6:.*]] = ada.binop "+" %[[V4]], %[[V5]] : !ada.qual<i32, @standard.integer>
-- MLIR-NEXT:    %[[C9:.*]] = ada.constant : !ada.qual<i32, @standard.integer> = 9
-- MLIR-NEXT:    %[[V7:.*]] = ada.binop "-" %[[V6]], %[[C9]] : !ada.qual<i32, @standard.integer>
-- MLIR-NEXT:    %[[C10:.*]] = ada.constant : !ada.qual<i32, @standard.integer> = 10
-- MLIR-NEXT:    %[[V8:.*]] = ada.binop "+" %[[V7]], %[[C10]] : !ada.qual<i32, @standard.integer>
-- MLIR-NEXT:    ada.return %[[V8]] : !ada.qual<i32, @standard.integer>

-- LLVM-LABEL: define i32 @_ada_literal_arithmetic(
-- LLVM:         ret i32 66

-- 1 + 2 + 3 + 4 + 5 - 6 + 7 * 8 - 9 + 10 = 66
function Literal_Arithmetic return Integer is
begin
   return 1 + 2 + 3 + 4 + 5 - 6 + 7 * 8 - 9 + 10;
end Literal_Arithmetic;
