-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- Reverse `for` loop (RM 5.5): starts at `hi`, tests `I = lo`, steps by an
-- unchecked `- 1` (a checked one would print `checks<overflow>`).
-- For_Loop_Reverse(5) = 15.

-- CHECK-LABEL: ada.subp @for_loop_reverse
-- CHECK:         ada.cmp "<="
-- CHECK:         ada.cmp "="
-- CHECK:         ada.binop "-" %{{[^ ]+}}, %{{[^ ]+}} : !ada.qual<i32, @standard.integer>
-- CHECK:         ada.return

function For_Loop_Reverse (N : Integer) return Integer is
   Sum : Integer := 0;
begin
   for I in reverse 1 .. N loop
      Sum := Sum + I;
   end loop;
   return Sum;
end For_Loop_Reverse;
