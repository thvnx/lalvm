-- RUN: %lalvm -O1 -g --emit=llvm %s | %FileCheck %s

-- Semantic promotion keeps uninitialized X visible as a debug value.

-- CHECK: #dbg_value(i32 0, ![[X:[0-9]+]]
-- CHECK-DAG: ![[X]] = !DILocalVariable(name: "x"

function Mem2reg_Dbg return Integer is
   X : Integer;
begin
   return X;
end Mem2reg_Dbg;
