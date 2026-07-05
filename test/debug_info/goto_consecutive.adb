-- RUN: %lalvm -g --emit=llvm %s | %llc -filetype=obj -o %t.o
-- RUN: %llvm-dwarfdump --debug-info %t.o | %FileCheck %s

-- Two consecutive goto labels each get their own DW_TAG_label. The first
-- label's block holds only the branch into the second's block, which anchors
-- the first label.

-- CHECK-DAG: DW_AT_name ("first")
-- CHECK-DAG: DW_AT_name ("second")

procedure Goto_Consecutive (X : in out Integer) is
begin
   if X > 0 then
      goto First;
   elsif X < 0 then
      goto Second;
   end if;
   X := X + 100;
   <<First>>
   <<Second>>
   X := X + 1;
end Goto_Consecutive;
