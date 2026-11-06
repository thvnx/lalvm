-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR --implicit-check-not=ada.alloca --implicit-check-not=memref.load --implicit-check-not=memref.store
-- RUN: %lalvm -O1 --emit=llvm %s | %FileCheck %s --check-prefix=LLVM
-- RUN: %lalvm -g --emit=llvm %s | %FileCheck %s --check-prefix=DI
-- RUN: %lalvm -g -O1 --emit=llvm %s | %FileCheck %s --check-prefix=DI

-- Forward `for` loop (RM 5.5): empty-range guard, body, then a pre-step
-- `I = hi` test so the induction step is unchecked (RM 5.5(9)).
-- For_Loop(5) = 15.

-- MLIR-LABEL: ada.subp @for_loop
-- MLIR:         ada.cmp "<="
-- MLIR:         cf.cond_br
-- MLIR:       ^{{bb[0-9]+}}(%[[I:[^: ]+]]: !ada.qual<i32, @standard.integer>):
-- MLIR:         ada.binop "+" %{{.*}}, %[[I]] checks<overflow>
-- MLIR:         ada.cmp "="
-- MLIR:         cf.cond_br
-- MLIR:         ada.return

-- LLVM-LABEL: define i32 @_ada_for_loop(
-- LLVM:         phi i32
-- LLVM:         ret i32

-- DI: %[[PHI:[0-9]+]] = phi i32
-- DI: #dbg_value(i32 %[[PHI]], ![[VAR:[0-9]+]],
-- DI-DAG: ![[VAR]] = !DILocalVariable(name: "i", scope: {{.*}}type: ![[TYPE:[0-9]+]])
-- DI-DAG: ![[TYPE]] = !DIBasicType(name: "integer", size: 32,

function For_Loop (N : Integer) return Integer is
   Sum : Integer := 0;
begin
   for I in 1 .. N loop
      Sum := Sum + I;
   end loop;
   return Sum;
end For_Loop;
