-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- Regression test: nested subprogram parameter shadows outer-scope variable
-- with the same name. Previously failed silently due to the string-based
-- symbol table rejecting the parameter declaration as a duplicate.

-- MLIR-LABEL: ada.func @p() -> i32
-- MLIR:         %[[I:.*]] = arith.constant 12 : i32
-- MLIR:         ada.func @inner(
-- MLIR:           %[[ONE:.*]] = arith.constant 1 : i32
-- MLIR:           %[[R:.*]] = ada.add %{{.*}}, %[[ONE]] : i32
-- MLIR:           ada.return %[[R]] : i32
-- MLIR:         %[[RES:.*]] = ada.call @inner(%[[I]]) : (i32) -> i32
-- MLIR-NEXT:    ada.return %[[RES]] : i32

-- LLVM-LABEL: define i32 @_ada_p(
-- LLVM:          call i32 @p__inner(i32 12)
-- LLVM:          ret i32
-- LLVM-LABEL: define i32 @p__inner(i32
-- LLVM:          add i32
-- LLVM:          ret i32

function P return Integer is
   I : Integer := 12;

   function Inner (I : Integer) return Integer;

   function Inner (I : Integer) return Integer is
   begin
      return I + 1;
   end Inner;
begin
   return Inner (I);
end P;
