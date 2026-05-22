-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- Regression test: nested subprogram parameter shadows outer-scope variable
-- with the same name. Previously failed silently due to the string-based
-- symbol table rejecting the parameter declaration as a duplicate.

-- MLIR-LABEL: ada.subp @p() -> i32
-- MLIR:         %[[I_INIT:.*]] = arith.constant {ada.type = @standard.integer} 12 : i32
-- MLIR-NEXT:    %[[I_PTR:.*]] = memref.alloca() {ada.type = @standard.integer} : memref<i32>
-- MLIR-NEXT:    memref.store %[[I_INIT]], %[[I_PTR]][] {ada.type = @standard.integer} : memref<i32>
-- MLIR:         ada.subp @inner(
-- MLIR:           %[[ONE:.*]] = arith.constant {ada.type = @standard.integer} 1 : i32
-- MLIR:           %[[R:.*]] = ada.binop "+" %{{.*}}, %[[ONE]] {ada.type = @standard.integer} : i32
-- MLIR:           ada.return %[[R]] : i32
-- MLIR:         %[[I:.*]] = memref.load %[[I_PTR]][] : memref<i32>
-- MLIR:         %[[RES:.*]] = ada.call @inner(%[[I]]) {ada.type = @standard.integer} : (i32) -> i32
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
