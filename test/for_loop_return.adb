-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- A `for` loop whose body never falls through: the pre-step `I = hi` test and
-- the step block are never reachable, so MLIRGen erases them; only the
-- empty-range guard's comparison remains.

-- CHECK-LABEL: ada.subp @for_loop_return
-- CHECK:         ada.cmp "<="
-- CHECK:         cf.cond_br
-- CHECK:         ada.return
-- CHECK-NOT:     ada.cmp "="

function For_Loop_Return return Integer is
begin
   for I in 1 .. 10 loop
      return 1;
   end loop;
   return 0;
end For_Loop_Return;
