-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- Regression test: nested subprogram calls itself recursively.
-- Previously failed because LowerToLLVM's replaceAllSymbolUses does not
-- walk into the symbol's own definition body, leaving the recursive
-- ada.call unrenamed and producing an invalid llvm.call reference.

-- MLIR-LABEL: ada.subp @p() -> !ada.qual<i32, @standard.integer>
-- MLIR:         ada.subp @inner(
-- MLIR:           %[[R:.*]] = ada.call @inner(%{{.*}}) : (!ada.qual<i32, @standard.integer>) -> !ada.qual<i32, @standard.integer>
-- MLIR:           ada.return %[[R]] : !ada.qual<i32, @standard.integer>
-- MLIR:         ada.call @inner(%{{.*}}) : (!ada.qual<i32, @standard.integer>) -> !ada.qual<i32, @standard.integer>

-- LLVM-LABEL: define i32 @_ada_p(
-- LLVM:          call i32 @p__inner(
-- LLVM-LABEL: define i32 @p__inner(i32
-- LLVM:          call i32 @p__inner(
-- LLVM:          ret i32

function P return Integer is
   I : Integer := 12;

   function Inner (I : Integer) return Integer;

   function Inner (I : Integer) return Integer is
   begin
      return Inner (I + 1);
   end Inner;
begin
   return Inner (I);
end P;
