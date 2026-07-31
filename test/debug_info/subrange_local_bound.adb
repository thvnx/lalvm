-- RUN: %lalvm -g --emit=llvm %s | %FileCheck %s

-- A dynamic bound held in a *local*: no dbg.value binds the loaded bound
-- value (that reuse works only for parameters), so AdaDebugInfoPass creates
-- the artificial `s'last` variable.

-- CHECK-DAG: !DISubrangeType(name: "s",{{.*}}lowerBound: i32 1, upperBound: ![[U:[0-9]+]])
-- CHECK-DAG: ![[U]] = !DILocalVariable(name: "s'last"{{.*}}flags: DIFlagArtificial)

function Subrange_Local_Bound (X : Integer) return Integer is
   N : Integer := X * 2;
   subtype S is Integer range 1 .. N;
   procedure Set (V : out S) is
   begin
      V := 1;
   end Set;
   V : S;
begin
   Set (V);
   return V;
end Subrange_Local_Bound;
