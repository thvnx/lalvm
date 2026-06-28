-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR

-- Backward `goto` (RM 5.8) forms a loop by hand: `<<Top>>` is the back-edge
-- target, `goto Fin` exits, `goto Top` re-enters. `<<Fin>>` follows the
-- `goto Top` terminator but is a live target, so it is still emitted.

-- MLIR-LABEL: ada.subp @goto_backward
-- MLIR:         cf.br ^[[TOP:bb[0-9]+]](
-- MLIR:       ^[[TOP]](
-- MLIR:         ada.cmp ">="
-- MLIR:         cf.cond_br %{{.*}}, ^[[EXIT:bb[0-9]+]], ^[[CONT:bb[0-9]+]]
-- MLIR:       ^[[EXIT]]:
-- MLIR:         cf.br ^[[FIN:bb[0-9]+]]
-- MLIR:       ^[[CONT]]:
-- MLIR:         ada.binop "+"
-- MLIR:         cf.br ^[[TOP]](
-- MLIR:       ^[[FIN]]:
-- MLIR:         ada.return

function Goto_Backward (N : Integer) return Integer is
   I : Integer := 0;
begin
   <<Top>>
   if I >= N then
      goto Fin;
   end if;
   I := I + 1;
   goto Top;
   <<Fin>>
   return I;
end Goto_Backward;
