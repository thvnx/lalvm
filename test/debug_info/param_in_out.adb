-- RUN: %lalvm -g --emit=llvm %s | %FileCheck %s

-- Writable formals are described on their local object, with source numbering.

-- CHECK-LABEL: define internal i32 @{{.*}}increment(
-- CHECK:         %[[OBJECT:.*]] = alloca i32
-- CHECK:         #dbg_declare(ptr %[[OBJECT]],
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
