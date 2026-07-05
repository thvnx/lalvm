-- RUN: %lalvm -g --emit=llvm %s | %FileCheck %s

-- CHECK: !DICompileUnit(language: DW_LANG_Ada2012
-- CHECK-SAME:           producer: "lalvm"

procedure Compile_Unit is
begin
   null;
end Compile_Unit;
