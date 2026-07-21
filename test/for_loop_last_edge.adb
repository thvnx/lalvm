-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- Attribute bounds (`Integer'Last`, RM 5.5) fold statically; the pre-step
-- bound test means the step never computes `Integer'Last + 1`, so it is an
-- unchecked `+`. For_Loop_Last_Edge = 3.

-- CHECK-LABEL: ada.subp @for_loop_last_edge
-- CHECK:         ada.constant : !ada.qual<i32, @standard.integer> = 2147483645
-- CHECK:         ada.constant : !ada.qual<i32, @standard.integer> = 2147483647
-- CHECK:         ada.cmp "<=" %{{.*}}, %{{.*}}
-- CHECK:         cf.cond_br %{{.*}}, ^[[INIT:bb[0-9]+]], ^[[MERGE:bb[0-9]+]]
-- CHECK:       ^[[INIT]]:
-- CHECK:         cf.br ^[[BODY:bb[0-9]+]]
-- CHECK:       ^[[BODY]]:
-- CHECK:         ada.binop "+" %{{.*}}, %{{.*}} checks<overflow>
-- CHECK:         ada.cmp "=" %{{.*}}, %{{.*}}
-- CHECK:         cf.cond_br %{{.*}}, ^[[MERGE]], ^[[STEP:bb[0-9]+]]
-- CHECK:       ^[[STEP]]:
-- CHECK:         ada.binop "+" %{{[^ ]+}}, %{{[^ ]+}} : !ada.qual<i32, @standard.integer>
-- CHECK:       ^[[MERGE]]:
-- CHECK:         ada.return

function For_Loop_Last_Edge return Integer is
   Count : Integer := 0;
begin
   for I in Integer'Last - 2 .. Integer'Last loop
      Count := Count + 1;
   end loop;
   return Count;
end For_Loop_Last_Edge;
