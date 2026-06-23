-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --implicit-check-not=DIDerivedType

-- A constrained subtype used as a parameter and return type is described by the
-- subrange in the subprogram's DISubroutineType (not the typedef placeholder),
-- and that placeholder is dropped entirely: `--implicit-check-not=DIDerivedType`
-- fails if any typedef stub leaks. `Id`'s signature is therefore the subrange
-- twice (return + the single parameter).

-- CHECK-DAG: ![[SUB:[0-9]+]] = !DISubrangeType(name: "s",{{.*}}lowerBound: i32 1, upperBound: i32 10)
-- CHECK-DAG: = !{![[SUB]], ![[SUB]]}

function Subrange_Sig return Integer is
   subtype S is Integer range 1 .. 10;
   function Id (X : S) return S is
   begin
      return X;
   end Id;
   V : S := 5;
begin
   return Id (V);
end Subrange_Sig;
