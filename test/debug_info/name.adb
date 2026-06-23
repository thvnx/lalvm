-- RUN: %lalvm --emit=llvm %s | %llc -filetype=obj -o %t.o
-- RUN: %llvm-dwarfdump --debug-info %t.o | %FileCheck %s

-- A subprogram DIE distinguishes the human-facing source name (DW_AT_name) from
-- the GNAT-mangled ABI symbol (DW_AT_linkage_name). The library-level function
-- gets the `_ada_` prefix; the nested procedure uses its simple source name with
-- a scope-qualified mangled linkage name.

-- CHECK-DAG: DW_AT_linkage_name ("_ada_name")
-- CHECK-DAG: DW_AT_name ("name")
-- CHECK-DAG: DW_AT_linkage_name ("name__helper")
-- CHECK-DAG: DW_AT_name ("helper")

function Name return Integer is
   procedure Helper is
   begin
      null;
   end Helper;
begin
   Helper;
   return 0;
end Name;
