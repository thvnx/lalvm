-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- Unary logical negation 'not' (RM 4.5.6) on a Boolean: modelled by ada.unop
-- and lowered to a one-bit complement (xor with true). The operand is a
-- parameter so it is not folded.

-- MLIR-LABEL: ada.subp @logical_not
-- MLIR:         ada.unop "not" %arg0 : !ada.qual<i1, @standard.boolean>

-- LLVM-LABEL: define i1 @_ada_logical_not(
-- LLVM:          xor i1 %0, true

function Logical_Not (X : Boolean) return Boolean is
begin
   return not X;
end Logical_Not;
