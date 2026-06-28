-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- Unary 'not' on a subtype of a modular type (RM 4.5.6): the modulus lives on
-- the base type, so MLIRGen walks `base` links rather than reading the subtype
-- (range only). `not` on `Tiny` (base `Small`, mod 100) is the one's complement
-- `99 - X`, not rejected as non-modular. `X` is a parameter so it is not folded.

-- MLIR: ada.unop "not" %arg0 : !ada.qual<i8, @{{.*}}tiny>

-- LLVM: sub i8 99, %0

function Not_Modular_Subtype return Boolean is
   type Small is mod 100;
   subtype Tiny is Small range 0 .. 50;
   function Flip (X : Tiny) return Small is
   begin
      return not X;
   end Flip;
begin
   return Flip (5) = 0;
end Not_Modular_Subtype;
