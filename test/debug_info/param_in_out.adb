-- RUN: %lalvm --emit=llvm %s | %FileCheck %s

-- An `in out` parameter is passed by reference, so it is described with
-- #dbg_declare on the incoming pointer, with a DILocalVariable arg: 1.

-- CHECK-LABEL: define internal void @{{.*}}increment(ptr
-- CHECK:         #dbg_declare(ptr %0,
-- CHECK:         DILocalVariable(name: "x", arg: 1,

function Param_In_Out return Integer is
   procedure Increment (X : in out Integer);

   procedure Increment (X : in out Integer) is
   begin
      X := X + 1;
   end Increment;
   N : Integer := 41;
begin
   Increment (N);
   return N;
end Param_In_Out;
