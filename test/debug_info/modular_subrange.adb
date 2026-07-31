-- RUN: %lalvm -g --emit=llvm %s | %llc -filetype=obj -o %t.o
-- RUN: %llvm-dwarfdump --debug-info %t.o | %FileCheck %s

-- A constrained subtype of a modular type: the subrange's base type keeps the
-- unsigned encoding (a signed default would misread values above T'Last / 2).

-- CHECK: DW_TAG_variable
-- CHECK: DW_AT_name ("x")
-- CHECK: DW_AT_type ({{.*}} "s")
-- CHECK: DW_TAG_subrange_type
-- CHECK: DW_AT_name ("s")
-- CHECK: DW_AT_lower_bound (1)
-- CHECK: DW_AT_upper_bound (100)
-- CHECK: DW_TAG_base_type
-- CHECK: DW_AT_name ("t")
-- CHECK: DW_AT_encoding (DW_ATE_unsigned)

procedure Modular_Subrange is
   type T is mod 128;
   subtype S is T range 1 .. 100;
   procedure Init (V : out S) is
   begin
      V := 1;
   end Init;
   X : S;
begin
   Init (X);
end Modular_Subrange;
