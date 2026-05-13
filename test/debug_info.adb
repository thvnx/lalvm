-- RUN: %lalvm --emit=llvm %s | %FileCheck %s

-- CHECK: !DICompileUnit(language: DW_LANG_Ada2012
-- CHECK-SAME:           producer: "lalvm"

procedure Debug_Info is
begin
   null;
end Debug_Info;
