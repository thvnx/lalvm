-- RUN: %lalvm --emit=mlir %s | %FileCheck %s
-- XFAIL: *

-- `for I in 1 .. N loop` (RM 5.5) is not supported: lalvm diagnoses the
-- for_loop_spec rather than lowering it, so this fails to compile.
-- XFAIL until `for` loops are supported, at which point the XPASS flags the
-- checks below for finalizing. For_Loop_Unsupported(5) = 15.

-- CHECK-LABEL: ada.subp @for_loop_unsupported
-- CHECK:         cf.cond_br
-- CHECK:         ada.binop "+"
-- CHECK:         ada.return

function For_Loop_Unsupported (N : Integer) return Integer is
   Sum : Integer := 0;
begin
   for I in 1 .. N loop
      Sum := Sum + I;
   end loop;
   return Sum;
end For_Loop_Unsupported;
