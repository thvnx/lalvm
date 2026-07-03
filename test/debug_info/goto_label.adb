-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM
-- RUN: %lalvm --emit=llvm %s | %llc -filetype=obj -o %t.o
-- RUN: %llvm-dwarfdump --debug-info %t.o | %FileCheck %s

-- A goto label (RM 5.8) produces a DW_TAG_label with its source name and a code
-- address (DW_AT_low_pc), matching GNAT. (Named loops and blocks get no label
-- DIE -- GNAT emits none.)

-- The LLVM IR carries a #dbg_label record referencing a named !DILabel node.
-- LLVM: #dbg_label(![[LABEL:[0-9]+]]
-- LLVM: ![[LABEL]] = !DILabel(scope: {{.*}}, name: "done"

-- CHECK: DW_TAG_label
-- CHECK: DW_AT_name ("done")
-- CHECK: DW_AT_low_pc

function Goto_Label (X : Integer) return Integer is
   R : Integer := X;
begin
   if R > 0 then
      goto Done;
   end if;
   R := R + 100;
   <<Done>>
   return R;
end Goto_Label;
