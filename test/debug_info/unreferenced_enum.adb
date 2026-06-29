-- RUN: %lalvm --emit=llvm %s | %FileCheck %s

-- A predefined type's DIE is emitted only when a surviving entity references it
-- (matching GNAT). Here the only Boolean is the transient result of `X = 0`
-- (the `if` condition); no variable, parameter, or return is Boolean, so no
-- `boolean` DW_TAG_enumeration_type is emitted.

-- CHECK: define i32 @_ada_unreferenced_enum(
-- CHECK-NOT: DW_TAG_enumeration_type
-- CHECK-NOT: name: "boolean"

function Unreferenced_Enum (X : Integer) return Integer is
begin
   if X = 0 then
      return 1;
   end if;
   return 2;
end Unreferenced_Enum;
