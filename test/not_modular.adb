-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- Unary 'not' on a modular type (RM 4.5.6) is one's complement within the
-- modulus, `(modulus - 1) - X`, lowered to a subtract (not the Boolean xor):
-- for `mod 100` that is `99 - X`. The operand is a parameter of the nested
-- `Flip` so the `not` is not folded.

-- MLIR: ada.unop "not" %arg0 : !ada.qual<i8, @{{.*}}>

-- LLVM: sub i8 99, %0

function Not_Modular return Boolean is
   type Small is mod 100;
   function Flip (X : Small) return Small is
   begin
      return not X;
   end Flip;
begin
   return Flip (5) = 0;
end Not_Modular;
