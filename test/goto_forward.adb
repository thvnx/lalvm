-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- Forward `goto` (RM 5.8): the `then` branch jumps to `Done`, skipping the
-- `X + 100`. Both the jump and the fall-through reach the one `Done` block.

-- MLIR-LABEL: ada.subp @goto_forward
-- MLIR:         cf.cond_br %{{.*}}, ^[[THEN:bb[0-9]+]], ^[[ELSE:bb[0-9]+]]
-- MLIR:       ^[[THEN]]:
-- MLIR:         cf.br ^[[DONE:bb[0-9]+]]
-- MLIR:       ^[[ELSE]]:
-- MLIR:         ada.binop "+"
-- MLIR:         cf.br ^[[DONE]]
-- MLIR:       ^[[DONE]]:
-- MLIR:         ada.binop "+"
-- MLIR:         ada.return

-- LLVM-LABEL: define void @_ada_goto_forward(
-- LLVM:         br i1
-- LLVM:         br label %[[DONE:[0-9]+]]
-- LLVM:       [[DONE]]:
-- LLVM:         ret void

procedure Goto_Forward (X : in out Integer) is
begin
   if X > 0 then
      goto Done;
   end if;
   X := X + 100;
   <<Done>>
   X := X + 1;
end Goto_Forward;
