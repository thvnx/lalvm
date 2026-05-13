-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.func @caller(
-- MLIR:         ada.func @callee(
-- MLIR:           ada.return %{{.*}} : i32
-- MLIR:         %[[V:.*]] = ada.call @callee(%arg0, %arg1) : (i32, i32) -> i32
-- MLIR:         ada.return %[[V]] : i32

-- LLVM-LABEL: define i32 @_ada_caller(
-- LLVM:          %{{.*}} = call i32 @caller__callee(
-- LLVM-LABEL: define i32 @caller__callee(

function Caller (I, J : Integer) return Integer is
   function Callee (X, Y : Integer) return Integer is
   begin
      return X + Y;
   end Callee;
begin
   return Callee (I, J);
end Caller;
