-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.subp @caller(
-- MLIR:         ada.subp @callee(
-- MLIR:           ada.return %{{.*}} : !ada.qual<i32, @standard.integer>
-- MLIR:         %[[V:.*]] = ada.call @callee(%arg0, %arg1) : (!ada.qual<i32, @standard.integer>, !ada.qual<i32, @standard.integer>) -> !ada.qual<i32, @standard.integer>
-- MLIR:         ada.return %[[V]] : !ada.qual<i32, @standard.integer>

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
