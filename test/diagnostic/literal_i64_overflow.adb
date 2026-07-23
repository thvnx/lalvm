-- RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

-- CHECK: error: value not in range of type "standard.long_integer"
-- CHECK: error: static expression fails Constraint_Check

-- 9_223_372_036_854_775_808 = 2^63 = Long_Integer'Last + 1, outside its range
function Literal_I64_Overflow return Long_Integer is
begin
   return 9_223_372_036_854_775_808;
end Literal_I64_Overflow;
