-- RUN: %lalvm -O1 --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm -O1 --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- If statement with no else (RM 5.3): the false edge goes to the merge block,
-- and the then-branch falls through to it (cf.br). The local R is live across
-- the branch, so mem2reg promotes it to a block argument (an LLVM phi),
-- exercising cf's BranchOpInterface across the new blocks.

-- MLIR-LABEL: ada.subp @if_no_else
-- MLIR:         ada.cmp "=" %arg0, %{{.*}} : (!ada.qual<i32, @standard.integer>, !ada.qual<i32, @standard.integer>) -> !ada.qual<i1, @standard.boolean>
-- MLIR:         ada.unwrap %{{.*}} : !ada.qual<i1, @standard.boolean> to i1
-- MLIR:         cf.cond_br
-- MLIR:         cf.br
-- MLIR:         ada.return

-- LLVM-LABEL: define i32 @_ada_if_no_else(
-- LLVM:         icmp eq i32 %0, 0
-- LLVM:         br i1
-- LLVM:         phi i32
-- LLVM:         ret i32

function If_No_Else (X : Integer) return Integer is
   R : Integer := X;
begin
   if X = 0 then
      R := 1;
   end if;
   return R;
end If_No_Else;
