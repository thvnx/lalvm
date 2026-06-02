-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.subp @func_call(
-- MLIR:         ada.subp @func_call.callee(
-- MLIR:           ada.return %{{.*}} : !ada.qual<i32, @standard.integer>
-- MLIR:         %[[V:.*]] = ada.call @func_call.callee(%arg0, %arg1) : (!ada.qual<i32, @standard.integer>, !ada.qual<i32, @standard.integer>) -> !ada.qual<i32, @standard.integer>
-- MLIR:         ada.return %[[V]] : !ada.qual<i32, @standard.integer>

-- LLVM-LABEL: define i32 @_ada_func_call(
-- LLVM:          %{{.*}} = call i32 @func_call__callee(
-- LLVM-LABEL: define i32 @func_call__callee(

function Func_Call (I, J : Integer) return Integer is
   function Callee (X, Y : Integer) return Integer is
   begin
      return X + Y;
   end Callee;
begin
   return Callee (I, J);
end Func_Call;
