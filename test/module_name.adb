-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- The MLIR module is named after the input file's basename without extension.
-- CHECK: module @module_name

procedure Module_Name is
begin
   null;
end Module_Name;
