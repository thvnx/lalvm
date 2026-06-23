-- RUN: %lalvm --emit=llvm %s | %FileCheck %s

-- The "Dwarf Version" module flag must be 5 so the backend emits the modern
-- `.debug_names` accelerator table instead of the deprecated `.debug_pubnames`.
-- CHECK: !{i32 7, !"Dwarf Version", i32 5}

procedure Dwarf_Version is
begin
   null;
end Dwarf_Version;
