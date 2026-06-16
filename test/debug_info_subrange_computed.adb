-- RUN: %lalvm --emit=llvm %s | %FileCheck %s

-- A *computed* dynamic bound (`N * 2`) has no source object to reference, so the
-- DISubrangeType's upper bound points at an artificial variable (`s'last`) that
-- AdaDebugInfoPass binds to the computed value via dbg.value; the static lower
-- bound stays the constant 1. `V` is passed `out` so it survives and keeps the
-- subrange reachable.

-- CHECK-DAG: !DISubrangeType(name: "s",{{.*}}lowerBound: i32 1, upperBound: ![[U:[0-9]+]])
-- CHECK-DAG: ![[U]] = !DILocalVariable(name: "s'last"{{.*}}flags: DIFlagArtificial)

function Debug_Info_Subrange_Computed (N : Integer) return Integer is
   subtype S is Integer range 1 .. N * 2;
   procedure Set (X : out S) is
   begin
      X := 1;
   end Set;
   V : S;
begin
   Set (V);
   return V;
end Debug_Info_Subrange_Computed;
