-- RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

-- CHECK: error: integer literal 9223372036854775808 out of range for i64

-- 9_223_372_036_854_775_808 = 2^63 = Long_Integer'Last + 1, does not fit in i64
function Test_I64_Overflow return Long_Integer is
begin
   return 9_223_372_036_854_775_808;
end Test_I64_Overflow;
