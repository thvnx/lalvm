-- RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

-- Unary `-` is not yet supported: `ada.unop` handles only `not` until its
-- kind enum grows `-`/`+`/`abs`.

-- CHECK: error: invalid unary operator

function Unary_Minus (X : Integer) return Integer is
begin
   return -X;
end Unary_Minus;
