-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- `in` parameters are passed by value: the callee receives a scalar SSA
-- argument with no alloca and no load/store.

-- MLIR-LABEL: ada.subp @test_param_in
-- MLIR:         ada.subp @double(%arg0: i32) -> i32
-- MLIR-NOT:       memref
-- MLIR:           %[[R:.*]] = ada.binop "+" %arg0, %arg0 : i32
-- MLIR:           ada.return %[[R]] : i32
-- MLIR:         %[[C:.*]] = arith.constant 21 : i32
-- MLIR:         %[[V:.*]] = ada.call @double(%[[C]]) : (i32) -> i32
-- MLIR:         ada.return %[[V]] : i32

-- LLVM-LABEL: define i32 @_ada_test_param_in(
-- LLVM-LABEL: define i32 @test_param_in__double(i32
-- LLVM-NOT:     alloca
-- LLVM:          #dbg_value(i32 %0,
-- LLVM:          add i32
-- LLVM:          ret i32
-- LLVM:          DILocalVariable(name: "x", arg: 1,

function Test_Param_In return Integer is
   function Double (X : in Integer) return Integer;

   function Double (X : in Integer) return Integer is
   begin
      return X + X;
   end Double;
begin
   return Double (21);
end Test_Param_In;
