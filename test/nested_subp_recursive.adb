-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- Regression test: nested subprogram calls itself recursively.
-- Previously failed because LowerToLLVM's replaceAllSymbolUses does not
-- walk into the symbol's own definition body, leaving the recursive
-- ada.call unrenamed and producing an invalid llvm.call reference.

-- MLIR-LABEL: ada.subp @nested_subp_recursive() -> !ada.qual<i32, @standard.integer>
-- MLIR:         ada.subp private @nested_subp_recursive.inner(
-- MLIR:           %[[R:.*]] = ada.call @nested_subp_recursive.inner(%{{.*}}) : (!ada.qual<i32, @standard.integer>) -> !ada.qual<i32, @standard.integer>
-- MLIR:           ada.return %[[R]] : !ada.qual<i32, @standard.integer>
-- MLIR:         ada.call @nested_subp_recursive.inner(%{{.*}}) : (!ada.qual<i32, @standard.integer>) -> !ada.qual<i32, @standard.integer>

-- LLVM-LABEL: define i32 @_ada_nested_subp_recursive(
-- LLVM:          call i32 @nested_subp_recursive__inner(
-- LLVM-LABEL: define internal i32 @nested_subp_recursive__inner(i32
-- LLVM:          call i32 @nested_subp_recursive__inner(
-- LLVM:          ret i32

function Nested_Subp_Recursive return Integer is
   I : Integer := 12;

   function Inner (I : Integer) return Integer;

   function Inner (I : Integer) return Integer is
   begin
      return Inner (I + 1);
   end Inner;
begin
   return Inner (I);
end Nested_Subp_Recursive;
