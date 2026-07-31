-- RUN: %lalvm -g --emit=llvm %s | %FileCheck %s

-- Mirror of `subrange_dynamic.adb`: the dynamic *lower* bound references the
-- bound object's variable; the static upper bound stays the constant 10.

-- CHECK-DAG: !DISubrangeType(name: "s",{{.*}}lowerBound: ![[N:[0-9]+]], upperBound: i32 10)
-- CHECK-DAG: ![[N]] = !DILocalVariable(name: "n", arg: 1,

function Subrange_Dynamic_Lower (N : Integer) return Integer is
   subtype S is Integer range N .. 10;
   procedure Set (V : out S) is
   begin
      V := 10;
   end Set;
   V : S;
begin
   Set (V);
   return V;
end Subrange_Dynamic_Lower;
