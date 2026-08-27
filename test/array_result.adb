-- RUN: %lalvm --emit=mlir %s 2>&1 | %FileCheck %s
-- XFAIL: *

-- A function returning an array type is not supported yet.

-- CHECK-NOT: error: returning an array is not supported

function Array_Result return String is
begin
   return "Hello Wolrd!";
end Array_Result;
