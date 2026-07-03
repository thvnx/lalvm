-- RUN: %lalvm --emit=llvm %s | %llc -filetype=obj -o %t.o
-- RUN: %llvm-dwarfdump --debug-info %t.o | %FileCheck %s

-- A trailing goto label at the end of a procedure (RM 5.1: a label may end a
-- sequence of statements with nothing after it). Its block is just the implicit
-- return, which anchors the DW_TAG_label.

-- CHECK: DW_TAG_label
-- CHECK: DW_AT_name ("done")
-- CHECK: DW_AT_low_pc

procedure Goto_Label_End (X : in out Integer) is
begin
   if X > 0 then
      goto Done;
   end if;
   X := X + 100;
   <<Done>>
end Goto_Label_End;
