-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm -O1 --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- Forward `for` loop (RM 5.5): empty-range guard, body, then a pre-step
-- `I = hi` test so the induction step is unchecked (RM 5.5(9)).
-- For_Loop(5) = 15.

-- MLIR-LABEL: ada.subp @for_loop
-- MLIR:         ada.cmp "<="
-- MLIR:         cf.cond_br
-- MLIR:         ada.cmp "="
-- MLIR:         cf.cond_br
-- MLIR:         ada.return

-- LLVM-LABEL: define i32 @_ada_for_loop(
-- LLVM:         phi i32
-- LLVM:         ret i32

function For_Loop (N : Integer) return Integer is
   Sum : Integer := 0;
begin
   for I in 1 .. N loop
      Sum := Sum + I;
   end loop;
   return Sum;
end For_Loop;
