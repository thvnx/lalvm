-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- A literal value beyond 64 bits now compiles via the APInt literal path
-- (2**70, well within Long_Long_Long_Integer's i128 range).
-- CHECK: ada.constant : !ada.qual<i128, @standard.long_long_long_integer> = 1180591620717411303424
function Literal_I128 return Long_Long_Long_Integer is
begin
   return 1_180_591_620_717_411_303_424;
end Literal_I128;
