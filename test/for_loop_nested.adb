-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- Nested `for` loops (RM 5.5): the inner loop nests inside the outer body,
-- each with its own guard, bound test, and unchecked step. For_Loop_Nested = 36.

-- CHECK-LABEL: ada.subp @for_loop_nested
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
-- CHECK:         ada.binop "*" %{{.*}}, %{{.*}} checks<overflow>
-- CHECK:         ada.binop "+" %{{.*}}, %{{.*}} checks<overflow>
-- CHECK:         ada.cmp "=" %{{.*}}, %{{.*}}
-- CHECK:         ada.binop "+" %{{[^ ]+}}, %{{[^ ]+}} : !ada.qual<i32, @standard.integer>
-- CHECK:         ada.cmp "=" %{{.*}}, %{{.*}}
-- CHECK:         ada.binop "+" %{{[^ ]+}}, %{{[^ ]+}} : !ada.qual<i32, @standard.integer>
-- CHECK:       ^[[OMERGE]]:
-- CHECK:         ada.return

function For_Loop_Nested return Integer is
   Sum : Integer := 0;
begin
   for I in 1 .. 3 loop
      for J in 1 .. 3 loop
         Sum := Sum + I * J;
      end loop;
   end loop;
   return Sum;
end For_Loop_Nested;
