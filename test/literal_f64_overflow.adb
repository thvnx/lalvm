-- RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

-- CHECK: error: real literal 1.0E309 out of range for f64

-- 1.0E309 > Long_Float'Last (~1.8E308), does not fit in f64
function Test_F64_Overflow return Long_Float is
begin
   return 1.0E309;
end Test_F64_Overflow;
