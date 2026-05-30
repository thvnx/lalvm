-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.subp @literal
-- MLIR:         %{{.*}} = ada.constant : !ada.qual<i32, @standard.integer> = 42
-- MLIR-NEXT:    ada.return %{{.*}} : !ada.qual<i32, @standard.integer>

-- LLVM-LABEL: define i32 @_ada_literal(
-- LLVM:         ret i32 42

function Literal return Integer is
begin
   return 42;
end Literal;
