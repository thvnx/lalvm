-- RUN: %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

-- CHECK: warning: file name does not match unit name, should be "right_name.adb"

procedure Right_Name is
begin
   null;
end Right_Name;
