-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- As `loop_scope`, but with an unnamed loop: the qualified name is identical
-- (@loop_scope_unnamed.b.step_range). A loop adds no naming segment whether or
-- not it is named, so the inner block's "b" scopes the subtype either way.
-- Loop_Scope_Unnamed(5) = 15.

-- CHECK-LABEL: ada.subp @loop_scope_unnamed
-- CHECK:         cf.cond_br
-- CHECK:         ada.type @loop_scope_unnamed.b.step_range base @standard.integer
-- CHECK-NOT:     @loop_scope_unnamed.l

function Loop_Scope_Unnamed (N : Integer) return Integer is
   Total : Integer := 0;
   I     : Integer := 0;
begin
   while I < N loop
      declare
         subtype Step_Range is Integer range 0 .. 1000;
         Step : Step_Range := I + 1;
      begin
         Total := Total + Step;
         I := Step;
      end;
   end loop;
   return Total;
end Loop_Scope_Unnamed;
