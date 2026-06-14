-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.subp @sub
-- MLIR:         %[[R:.*]] = ada.binop "-" %arg0, %arg1 checks<overflow> : !ada.qual<i32, @standard.integer>
-- MLIR-NEXT:    ada.return %[[R]] : !ada.qual<i32, @standard.integer>

-- LLVM-LABEL: define i32 @_ada_sub(
-- LLVM:         %{{.*}} = sub i32 %0, %1
-- LLVM:         ret i32

function Sub (A, B : Integer) return Integer is
begin
   return A - B;
end Sub;
