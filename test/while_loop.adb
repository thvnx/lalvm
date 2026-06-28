-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- while loop (RM 5.5): a header block tests the condition and branches to the
-- body or the merge; the body branches back to the header. While_Loop(5) = 15.

-- MLIR-LABEL: ada.subp @while_loop
-- MLIR:         cf.br ^[[HDR:bb[0-9]+]](
-- MLIR:       ^[[HDR]](
-- MLIR:         %[[C:.*]] = ada.cmp "<=" %{{.*}}, %arg0
-- MLIR:         %[[I1:.*]] = ada.unwrap %[[C]]
-- MLIR:         cf.cond_br %[[I1]], ^[[BODY:bb[0-9]+]], ^[[MERGE:bb[0-9]+]]
-- MLIR:       ^[[BODY]]:
-- MLIR:         ada.binop "+"
-- MLIR:         cf.br ^[[HDR]](
-- MLIR:       ^[[MERGE]]:
-- MLIR:         ada.return

-- LLVM-LABEL: define i32 @_ada_while_loop(
-- LLVM:         phi i32
-- LLVM:         icmp sle i32 %{{.*}}, %0
-- LLVM:         br i1
-- LLVM:         ret i32

function While_Loop (N : Integer) return Integer is
   Sum : Integer := 0;
   I   : Integer := 1;
begin
   while I <= N loop
      Sum := Sum + I;
      I := I + 1;
   end loop;
   return Sum;
end While_Loop;
