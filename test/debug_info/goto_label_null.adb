-- RUN: %lalvm -g --emit=llvm %s | %llc -filetype=obj -o %t.o
-- RUN: %llvm-dwarfdump --debug-info %t.o | %FileCheck %s

-- A goto label on a null statement (RM 5.1). `ada.null` is erased in lowering,
-- so the label anchors on the next surviving op (here the implicit return)
-- rather than being dropped along with it. Still yields a DW_TAG_label.

-- CHECK: DW_TAG_label
-- CHECK: DW_AT_name ("done")
-- CHECK: DW_AT_low_pc

procedure Goto_Label_Null (X : in out Integer) is
begin
   if X > 0 then
      goto Done;
   end if;
   X := X + 100;
   <<Done>>
   null;
end Goto_Label_Null;
