-- RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

-- CHECK: error: integer literal 2147483648 out of range for i32

-- 2_147_483_648 = 2^31 = Integer'Last + 1, does not fit in i32
function Test_I32_Overflow return Integer is
begin
   return 2_147_483_648;
end Test_I32_Overflow;
