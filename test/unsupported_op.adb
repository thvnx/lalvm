-- RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

-- CHECK: invalid binary operator

function Test_Unsupported_Op (A, B : Integer) return Integer is
begin
   return A / B;
end Test_Unsupported_Op;
