-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- A named while loop whose body is a block declaring a subtype. A loop is not a
-- naming scope (it has no declarative part), so it contributes no segment to
-- qualified names: the inner block's "b" scopes the subtype, giving
-- @loop_scope.b.step_range (matching GNAT's loop_scope__B_1__step_range). The
-- loop name `Outer` is used only for `exit Outer`, never in a symbol name.
-- Loop_Scope(5) = 15.

-- CHECK-LABEL: ada.subp @loop_scope
-- CHECK:         cf.cond_br
-- CHECK:         ada.type @loop_scope.b.step_range base @standard.integer
-- CHECK-NOT:     @loop_scope.outer.

function Loop_Scope (N : Integer) return Integer is
   Total : Integer := 0;
   I     : Integer := 0;
begin
   Outer : while I < N loop
      declare
         subtype Step_Range is Integer range 0 .. 1000;
         Step : Step_Range := I + 1;
      begin
         Total := Total + Step;
         I := Step;
      end;
   end loop Outer;
   return Total;
end Loop_Scope;
