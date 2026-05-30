-- RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

-- CHECK: error: real constant {{.*}} out of range for f32

-- 1.0E39 > Float'Last (~3.4E38), does not fit in f32
function Literal_F32_Overflow return Float is
begin
   return 1.0E39;
end Literal_F32_Overflow;
