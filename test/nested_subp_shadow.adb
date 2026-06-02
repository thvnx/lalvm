-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- Regression test for symbol shadowing: a nested subprogram shares the
-- enclosing subprogram's name, and its parameter shadows an outer-scope
-- variable of the same name. The nested subprogram gets a distinct qualified
-- symbol and the call resolves to it by node identity, not to the enclosing
-- subprogram.

-- MLIR-LABEL: ada.subp @nested_subp_shadow() -> !ada.qual<i32, @standard.integer>
-- MLIR:         %[[I:.*]] = ada.constant : !ada.qual<i32, @standard.integer> = 12
-- MLIR:         ada.subp @nested_subp_shadow.nested_subp_shadow(
-- MLIR:           %[[ONE:.*]] = ada.constant : !ada.qual<i32, @standard.integer> = 1
-- MLIR:           %[[R:.*]] = ada.binop "+" %{{.*}}, %[[ONE]] : !ada.qual<i32, @standard.integer>
-- MLIR:           ada.return %[[R]] : !ada.qual<i32, @standard.integer>
-- MLIR:         %[[RES:.*]] = ada.call @nested_subp_shadow.nested_subp_shadow(%[[I]]) : (!ada.qual<i32, @standard.integer>) -> !ada.qual<i32, @standard.integer>
-- MLIR-NEXT:    ada.return %[[RES]] : !ada.qual<i32, @standard.integer>

-- LLVM-LABEL: define i32 @_ada_nested_subp_shadow(
-- LLVM:          call i32 @nested_subp_shadow__nested_subp_shadow(i32 12)
-- LLVM:          ret i32
-- LLVM-LABEL: define i32 @nested_subp_shadow__nested_subp_shadow(i32
-- LLVM:          add i32
-- LLVM:          ret i32

function Nested_Subp_Shadow return Integer is
   I : Integer := 12;

   function Nested_Subp_Shadow (I : Integer) return Integer;

   function Nested_Subp_Shadow (I : Integer) return Integer is
   begin
      return I + 1;
   end Nested_Subp_Shadow;
begin
   return Nested_Subp_Shadow (I);
end Nested_Subp_Shadow;
