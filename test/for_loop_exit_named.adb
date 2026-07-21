-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- `exit Outer` from an inner `for` loop (RM 5.7): the exit branches straight
-- to the *outer* merge, past the inner loop. For_Loop_Exit_Named = 5.

-- CHECK-LABEL: ada.subp @for_loop_exit_named
-- CHECK:         ada.cmp "<=" %{{.*}}, %{{.*}}
-- CHECK:         cf.cond_br %{{.*}}, ^[[OINIT:bb[0-9]+]], ^[[OMERGE:bb[0-9]+]]
-- CHECK:       ^[[OINIT]]:
-- CHECK:         cf.br ^[[OBODY:bb[0-9]+]]
-- CHECK:       ^[[OBODY]]:
-- CHECK:         ada.cmp "<=" %{{.*}}, %{{.*}}
-- CHECK:         cf.cond_br %{{.*}}, ^[[IINIT:bb[0-9]+]], ^{{bb[0-9]+}}
-- CHECK:       ^[[IINIT]]:
-- CHECK:         cf.br ^[[IBODY:bb[0-9]+]]
-- CHECK:       ^[[IBODY]]:
-- CHECK:         ada.binop "+" %{{.*}}, %{{.*}} checks<overflow>
-- CHECK:         ada.cmp "=" %{{.*}}, %{{.*}}
-- CHECK:         cf.cond_br %{{.*}}, ^[[OMERGE]], ^{{bb[0-9]+}}
-- CHECK:       ^[[OMERGE]]:
-- CHECK:         ada.return

function For_Loop_Exit_Named return Integer is
   Count : Integer := 0;
begin
   Outer :
   for I in 1 .. 10 loop
      for J in 1 .. 10 loop
         Count := Count + 1;
         exit Outer when Count = 5;
      end loop;
   end loop Outer;
   return Count;
end For_Loop_Exit_Named;
