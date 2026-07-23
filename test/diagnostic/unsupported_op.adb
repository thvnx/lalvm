-- RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

-- CHECK: invalid binary operator

function Unsupported_Op (A, B : Integer) return Integer is
begin
   return A ** B;
end Unsupported_Op;
