-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- If statement (RM 5.3) where the then-branch returns but the else-branch does
-- not: the then-block ends in ada.return (no fall-through), while the
-- else-block falls through (cf.br) to the merge block. The merge block is kept
-- and the statement after the if (return X) is emitted into it.

-- MLIR-LABEL: ada.subp @if_then_returns
-- MLIR:         cf.cond_br %{{.*}}, ^[[THEN:bb[0-9]+]], ^[[ELSE:bb[0-9]+]]
-- MLIR:       ^[[THEN]]:
-- MLIR:         ada.return %{{.*}} : !ada.qual<i32, @standard.integer>
-- MLIR:       ^[[ELSE]]:
-- MLIR:         cf.br ^[[MERGE:bb[0-9]+]]
-- MLIR:       ^[[MERGE]]:
-- MLIR:         ada.return %arg0 : !ada.qual<i32, @standard.integer>

-- LLVM-LABEL: define i32 @_ada_if_then_returns(
-- LLVM:         br i1
-- LLVM:         ret i32 100
-- LLVM:         br label %{{.*}}
-- LLVM:         ret i32 %0

function If_Then_Returns (X : Integer) return Integer is
begin
   if X = 0 then
      return 100;
   else
      null;
   end if;
   return X;
end If_Then_Returns;
