-- RUN: %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

-- A statically null range runs zero iterations (RM 5.5): warn and still emit
-- the loop (the empty-range guard skips it).

-- CHECK: warning: loop range is null, loop will not execute
-- CHECK: ada.subp @for_loop_null_range

function For_Loop_Null_Range return Integer is
   R : Integer := 99;
begin
   for I in 5 .. 1 loop
      R := I;
   end loop;
   return R;
end For_Loop_Null_Range;
