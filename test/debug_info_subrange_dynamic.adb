-- RUN: %lalvm --emit=llvm %s | %FileCheck %s

-- A dynamic subtype bound (`range 1 .. N`) points the DISubrangeType's upper
-- bound at the bound object's own variable (`N`), as GNAT does, rather than an
-- artificial copy; the static lower bound stays the constant 1. `V` is passed
-- `out` so it survives and carries the subrange type, keeping it reachable.

-- CHECK-DAG: !DISubrangeType(name: "s",{{.*}}lowerBound: i32 1, upperBound: ![[N:[0-9]+]])
-- CHECK-DAG: ![[N]] = !DILocalVariable(name: "n", arg: 1,

function Debug_Info_Subrange_Dynamic (N : Integer) return Integer is
   subtype S is Integer range 1 .. N;
   procedure Set (X : out S) is
   begin
      X := 1;
   end Set;
   V : S;
begin
   Set (V);
   return V;
end Debug_Info_Subrange_Dynamic;
