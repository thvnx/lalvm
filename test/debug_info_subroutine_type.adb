-- RUN: %lalvm --emit=llvm %s | %FileCheck %s

-- The subprogram's DISubroutineType lists the return type as element 0,
-- followed by the parameter types. For `function F (X : Integer) return
-- Integer` both are `integer`, so the types tuple holds the same node twice
-- (DW_AT_type on the subprogram is element 0). This pins the signature at the
-- IR level, which dwarfdump cannot show: parameter types in the subroutine
-- type are not rendered as subprogram attributes.

-- CHECK: !DISubroutineType(cc: DW_CC_normal, types: ![[TYPES:[0-9]+]])
-- CHECK: ![[TYPES]] = !{![[INT:[0-9]+]], ![[INT]]}
-- CHECK: ![[INT]] = !DIBasicType(name: "integer", size: 32, encoding: DW_ATE_signed)

function Debug_Info_Subroutine_Type (X : Integer) return Integer is
begin
   return X;
end Debug_Info_Subroutine_Type;
