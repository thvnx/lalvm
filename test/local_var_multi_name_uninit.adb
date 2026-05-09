-- RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

-- CHECK: variable 'Z' is read but never assigned

function Test_Local_Var_Multi_Name_Uninit return Integer is
   X, Y, Z : Integer;
begin
   return Z;
end Test_Local_Var_Multi_Name_Uninit;
