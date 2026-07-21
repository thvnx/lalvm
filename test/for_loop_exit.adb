-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- `exit when` from a `for` loop (RM 5.7) branches to the merge. Cmps in
-- order: range guard `<=`, exit test `>`, bound test `=`.

-- CHECK-LABEL: ada.subp @for_loop_exit
-- CHECK:         ada.cmp "<="
-- CHECK:         cf.cond_br
-- CHECK:         ada.cmp ">"
-- CHECK:         cf.cond_br
-- CHECK:         ada.cmp "="
-- CHECK:         cf.cond_br
-- CHECK:         ada.return

function For_Loop_Exit (N : Integer) return Integer is
   Sum : Integer := 0;
begin
   for I in 1 .. N loop
      Sum := Sum + I;
      exit when Sum > 100;
   end loop;
   return Sum;
end For_Loop_Exit;
