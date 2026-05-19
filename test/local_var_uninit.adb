-- RUN: %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

-- CHECK: variable 'X' is read before first assignment

function Test_Local_Var_Uninit return Integer is
   X : Integer;
begin
   return X;
end Test_Local_Var_Uninit;
