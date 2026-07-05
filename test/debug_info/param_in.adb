-- RUN: %lalvm -O1 -g --emit=llvm %s | %FileCheck %s

-- An `in` scalar parameter is passed by value, so the debugger describes it
-- with #dbg_value (a plain SSA value) rather than #dbg_declare, while still
-- emitting a DILocalVariable with arg: 1.

-- CHECK-LABEL: define internal i32 @{{.*}}double(i32
-- CHECK:         #dbg_value(i32 %0,
-- CHECK:         DILocalVariable(name: "x", arg: 1,

function Param_In return Integer is
   function Double (X : in Integer) return Integer;

   function Double (X : in Integer) return Integer is
   begin
      return X + X;
   end Double;
begin
   return Double (21);
end Param_In;
