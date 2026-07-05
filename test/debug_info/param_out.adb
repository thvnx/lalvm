-- RUN: %lalvm -g --emit=llvm %s | %FileCheck %s

-- An `out` parameter is passed by reference, so it is described with
-- #dbg_declare on the incoming pointer, with a DILocalVariable arg: 1.

-- CHECK-LABEL: define internal void @{{.*}}get_value(ptr
-- CHECK:         #dbg_declare(ptr %0,
-- CHECK:         DILocalVariable(name: "x", arg: 1,

function Param_Out return Integer is
   procedure Get_Value (X : out Integer);

   procedure Get_Value (X : out Integer) is
   begin
      X := 42;
   end Get_Value;
   N : Integer;
begin
   Get_Value (N);
   return N;
end Param_Out;
