-- RUN: %not %lalvm --emit=ast %s 2>&1 | %FileCheck %s

-- CHECK: parse_diag.adb:6:1: error: Missing ';'

function Parse_Diag return Integer
begin
   return 0;
end Parse_Diag;
