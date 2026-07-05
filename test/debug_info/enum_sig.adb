-- RUN: %lalvm -g --emit=llvm %s | %FileCheck %s --implicit-check-not='name: "enum_sig.color"'

-- An enum used as a parameter and return type is described by the full
-- DW_TAG_enumeration_type in the subprogram's DISubroutineType, not a
-- declaration-only stub: buildEnumDITypes rewrites signature types as well as
-- variable types, so no empty enum stub named by the full sym_name
-- ("enum_sig.color") leaks. `Id`'s signature is the full enum twice (return +
-- the one parameter).

-- CHECK-DAG: ![[E:[0-9]+]] = {{.*}}!DICompositeType(tag: DW_TAG_enumeration_type, name: "color"{{.*}}elements:
-- CHECK-DAG: = !{![[E]], ![[E]]}

function Enum_Sig return Integer is
   type Color is (Red, Green, Blue);
   function Id (X : Color) return Color is
   begin
      return X;
   end Id;
   V : Color := Green;
begin
   if Id (V) = Red then
      return 1;
   end if;
   return 0;
end Enum_Sig;
