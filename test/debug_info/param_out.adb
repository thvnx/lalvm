-- RUN: %lalvm -g --emit=llvm %s | %FileCheck %s

-- Writable formals are described on their local object, with source numbering.

-- CHECK-LABEL: define internal i32 @{{.*}}get_value(
-- CHECK:         %[[OBJECT:.*]] = alloca i32
-- CHECK:         #dbg_declare(ptr %[[OBJECT]],
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
