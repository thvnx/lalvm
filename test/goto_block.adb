-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- `goto` out of a nested block (RM 5.8). The `declare ... begin ... end` block
-- dissolves into the enclosing region, so the jump to `Done` is a single
-- `cf.br` across the (former) block boundary, no region crossing.

-- CHECK-LABEL: ada.subp @goto_block
-- CHECK:         cf.cond_br %{{.*}}, ^[[THEN:bb[0-9]+]], ^{{bb[0-9]+}}
-- CHECK:       ^[[THEN]]:
-- CHECK:         cf.br ^[[DONE:bb[0-9]+]]
-- CHECK:       ^[[DONE]]:
-- CHECK:         ada.return

procedure Goto_Block (X : in out Integer) is
begin
   declare
      Y : Integer := X;
   begin
      if Y > 0 then
         goto Done;
      end if;
   end;
   X := X + 100;
   <<Done>>
   X := X + 1;
end Goto_Block;
