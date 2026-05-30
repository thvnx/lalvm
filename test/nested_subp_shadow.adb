-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- Regression test: nested subprogram parameter shadows outer-scope variable
-- with the same name. Previously failed silently due to the string-based
-- symbol table rejecting the parameter declaration as a duplicate.

-- MLIR-LABEL: ada.subp @nested_subp_shadow() -> !ada.qual<i32, @standard.integer>
-- MLIR:         %[[I:.*]] = ada.constant : !ada.qual<i32, @standard.integer> = 12
-- MLIR:         ada.subp @inner(
-- MLIR:           %[[ONE:.*]] = ada.constant : !ada.qual<i32, @standard.integer> = 1
-- MLIR:           %[[R:.*]] = ada.binop "+" %{{.*}}, %[[ONE]] : !ada.qual<i32, @standard.integer>
-- MLIR:           ada.return %[[R]] : !ada.qual<i32, @standard.integer>
-- MLIR:         %[[RES:.*]] = ada.call @inner(%[[I]]) : (!ada.qual<i32, @standard.integer>) -> !ada.qual<i32, @standard.integer>
-- MLIR-NEXT:    ada.return %[[RES]] : !ada.qual<i32, @standard.integer>

-- LLVM-LABEL: define i32 @_ada_nested_subp_shadow(
-- LLVM:          call i32 @nested_subp_shadow__inner(i32 12)
-- LLVM:          ret i32
-- LLVM-LABEL: define i32 @nested_subp_shadow__inner(i32
-- LLVM:          add i32
-- LLVM:          ret i32

function Nested_Subp_Shadow return Integer is
   I : Integer := 12;

   function Inner (I : Integer) return Integer;

   function Inner (I : Integer) return Integer is
   begin
      return I + 1;
   end Inner;
begin
   return Inner (I);
end Nested_Subp_Shadow;
