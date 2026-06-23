-- RUN: %lalvm --emit=llvm %s | %llc -filetype=obj -o %t.o
-- RUN: %llvm-dwarfdump --debug-info %t.o | %FileCheck %s

-- A constrained integer subtype is described by a DW_TAG_subrange_type of its
-- base (with DW_AT_lower_bound / DW_AT_upper_bound), so a debugger shows the
-- constraint (`range 1 .. 10`) rather than a bare integer. Built by
-- buildSubrangeDITypes after translation, replacing the placeholder emitted by
-- AdaDebugInfoPass. `V` is passed `out` so it survives mem2reg as a real
-- variable carrying the subrange type. The by-reference parameter `X` of the
-- subtype is described by the same subrange (its Ada subtype comes from the
-- parameter's memref element, not from re-inferring the base type).

-- CHECK: DW_TAG_variable
-- CHECK: DW_AT_name ("v")
-- CHECK: DW_AT_type ({{.*}} "s")
-- CHECK: DW_TAG_subrange_type
-- CHECK: DW_AT_name ("s")
-- CHECK: DW_AT_lower_bound (1)
-- CHECK: DW_AT_upper_bound (10)
-- CHECK: DW_TAG_formal_parameter
-- CHECK: DW_AT_name ("x")
-- CHECK: DW_AT_type ({{.*}} "s")

function Subrange return Integer is
   subtype S is Integer range 1 .. 10;
   procedure Set (X : out S) is
   begin
      X := 5;
   end Set;
   V : S;
begin
   Set (V);
   return V;
end Subrange;
