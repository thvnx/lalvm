-- RUN: %lalvm -O1 --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm -O1 --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- named loop + `exit Name when` from an inner (unnamed) loop (RM 5.7). The
-- `exit Outer` targets the *outer* merge: the cond_br's true edge reaches the
-- function exit while its false edge back-edges to the inner body. The inner
-- loop's own merge is unreachable and is dropped. Exit_Named(5) = 5.

-- MLIR-LABEL: ada.subp @exit_named
-- MLIR:         cf.br ^[[OUTER:bb[0-9]+]]
-- MLIR:       ^[[OUTER]]:
-- MLIR:         cf.br ^[[INNER:bb[0-9]+]](
-- MLIR:       ^[[INNER]](
-- MLIR:         ada.binop "+"
-- MLIR:         %[[C:.*]] = ada.cmp ">=" %{{.*}}, %arg0
-- MLIR:         %[[I1:.*]] = ada.unwrap %[[C]]
-- MLIR:         cf.cond_br %[[I1]], ^[[OMERGE:bb[0-9]+]], ^[[ICONT:bb[0-9]+]]
-- MLIR:       ^[[ICONT]]:
-- MLIR:         cf.br ^[[INNER]](
-- MLIR:       ^[[OMERGE]]:
-- MLIR:         ada.return

-- LLVM-LABEL: define i32 @_ada_exit_named(
-- LLVM:         phi i32
-- LLVM:         icmp sge i32 %{{.*}}, %0
-- LLVM:         br i1
-- LLVM:         ret i32

function Exit_Named (N : Integer) return Integer is
   Count : Integer := 0;
begin
   Outer : loop
      loop
         Count := Count + 1;
         exit Outer when Count >= N;
      end loop;
   end loop Outer;
   return Count;
end Exit_Named;
