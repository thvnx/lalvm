-- RUN: %lalvm -O1 -g --emit=llvm %s | %FileCheck %s

-- `-g -O1`: the read before assignment is replaced by the stand-in value SSA
-- requires (`AllocaOp::getDefaultValue`; Ada gives an uninitialized scalar no
-- value, so any result conforms, RM 13.9.1). Built at the alloca's location,
-- it inherits the NameLoc, so `X` survives as a `#dbg_value`. An
-- *initialized* local loses its identity at -O1: its init carries no NameLoc.

-- CHECK: #dbg_value(i32 0, ![[X:[0-9]+]]
-- CHECK-DAG: ![[X]] = !DILocalVariable(name: "x"

function Mem2reg_Dbg return Integer is
   X : Integer;
begin
   return X;
end Mem2reg_Dbg;
