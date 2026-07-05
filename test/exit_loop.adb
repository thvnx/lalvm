-- RUN: %lalvm -O1 --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm -O1 --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- bare loop + `exit when` (RM 5.5/5.7). A bare loop has no header: the body is
-- its own back-edge target, reached again unless `exit` branches to the merge.
-- `exit when C` branches to the merge when C holds and continues otherwise.
-- Exit_Loop(5) = 5.

-- MLIR-LABEL: ada.subp @exit_loop
-- MLIR:         cf.br ^[[BODY:bb[0-9]+]](
-- MLIR:       ^[[BODY]](
-- MLIR:         %[[C:.*]] = ada.cmp ">=" %{{.*}}, %arg0
-- MLIR:         %[[I1:.*]] = ada.unwrap %[[C]]
-- MLIR:         cf.cond_br %[[I1]], ^[[MERGE:bb[0-9]+]], ^[[CONT:bb[0-9]+]]
-- MLIR:       ^[[CONT]]:
-- MLIR:         ada.binop "+"
-- MLIR:         cf.br ^[[BODY]](
-- MLIR:       ^[[MERGE]]:
-- MLIR:         ada.return

-- LLVM-LABEL: define i32 @_ada_exit_loop(
-- LLVM:         phi i32
-- LLVM:         icmp sge i32 %{{.*}}, %0
-- LLVM:         br i1
-- LLVM:         ret i32

function Exit_Loop (N : Integer) return Integer is
   I : Integer := 0;
begin
   loop
      exit when I >= N;
      I := I + 1;
   end loop;
   return I;
end Exit_Loop;
