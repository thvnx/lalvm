-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- A named number whose value exceeds int64 is now stored exactly via the
-- APInt path, in the universal_integer i512 carrier (it was silently dropped
-- before). 1180591620717411303424 = 2**70.
-- CHECK: ada.constant : !ada.qual<i512, @standard.universal_int_type_> = 1180591620717411303424
function Number_Decl_Wide return Long_Long_Long_Integer is
   Big : constant := 1180591620717411303424;
begin
   return Big;
end Number_Decl_Wide;
