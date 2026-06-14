-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- `in` parameters are passed by value: the callee receives a scalar SSA
-- argument with no alloca and no load/store.

-- MLIR-LABEL: ada.subp @param_in
-- MLIR:         ada.subp private @param_in.double(%arg0: !ada.qual<i32, @standard.integer>) -> !ada.qual<i32, @standard.integer>
-- MLIR-NOT:       memref
-- MLIR:           %[[R:.*]] = ada.binop "+" %arg0, %arg0 checks<overflow> : !ada.qual<i32, @standard.integer>
-- MLIR:           ada.return %[[R]] : !ada.qual<i32, @standard.integer>
-- MLIR:         %[[C:.*]] = ada.constant : !ada.qual<i32, @standard.integer> = 21
-- MLIR:         %[[V:.*]] = ada.call @param_in.double(%[[C]]) : (!ada.qual<i32, @standard.integer>) -> !ada.qual<i32, @standard.integer>
-- MLIR:         ada.return %[[V]] : !ada.qual<i32, @standard.integer>

-- LLVM-LABEL: define i32 @_ada_param_in(
-- LLVM-LABEL: define internal i32 @param_in__double(i32
-- LLVM-NOT:     alloca
-- LLVM:          #dbg_value(i32 %0,
-- LLVM:          add i32
-- LLVM:          ret i32
-- LLVM:          DILocalVariable(name: "x", arg: 1,

function Param_In return Integer is
   function Double (X : in Integer) return Integer;

   function Double (X : in Integer) return Integer is
   begin
      return X + X;
   end Double;
begin
   return Double (21);
end Param_In;
