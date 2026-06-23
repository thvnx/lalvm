-- RUN: %lalvm --emit=llvm %s | %llc -filetype=obj -o %t.o
-- RUN: %llvm-dwarfdump --debug-info %t.o | %FileCheck %s

-- An `in` parameter of a constrained subtype is a read-only view, so its DWARF
-- type is a DW_TAG_const_type wrapping the subtype's DW_TAG_subrange_type. The
-- subrange is resolved post-translation by buildSubrangeDITypes, which looks
-- through the const wrapper to swap the placeholder for the real subrange and
-- re-applies const to the result.

-- CHECK: DW_TAG_subrange_type
-- CHECK: DW_AT_name ("s")
-- CHECK: DW_AT_lower_bound (1)
-- CHECK: DW_AT_upper_bound (10)
-- CHECK: DW_TAG_formal_parameter
-- CHECK: DW_AT_name ("x")
-- CHECK: DW_AT_type ({{.*}} "const s")

function Param_Const return Integer is
   subtype S is Integer range 1 .. 10;
   function Id (X : S) return S is
   begin
      return X;
   end Id;
begin
   return Id (5);
end Param_Const;
