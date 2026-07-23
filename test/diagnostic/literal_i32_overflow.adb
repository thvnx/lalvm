-- RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

-- CHECK: error: value not in range of type "standard.integer"
-- CHECK: error: static expression fails Constraint_Check

-- 2_147_483_648 = 2^31 = Integer'Last + 1, outside Integer's static range
function Literal_I32_Overflow return Integer is
begin
   return 2_147_483_648;
end Literal_I32_Overflow;
