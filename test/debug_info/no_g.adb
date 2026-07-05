-- RUN: %lalvm --emit=obj %s -o %t.o
-- RUN: %llvm-dwarfdump --debug-info %t.o | %FileCheck %s --implicit-check-not=DW_TAG

-- Without -g (the default), no debug info reaches the object.

-- CHECK: file format

function No_G (X : Integer) return Integer is
   Y : Integer := X;
begin
   return Y;
end No_G;
