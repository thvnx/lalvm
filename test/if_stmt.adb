-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- If statement (RM 5.3) with an else, both branches returning. The Boolean
-- condition is unwrapped to i1 (ada.unwrap) and lowered to cf.cond_br. Neither
-- branch falls through, so no merge block is created.

-- MLIR-LABEL: ada.subp @if_stmt
-- MLIR:         %[[C:.*]] = ada.unwrap %arg0 : !ada.qual<i1, @standard.boolean> to i1
-- MLIR:         cf.cond_br %[[C]], ^[[T:bb[0-9]+]], ^[[E:bb[0-9]+]]
-- MLIR:       ^[[T]]:
-- MLIR:         ada.return %arg1 : !ada.qual<i32, @standard.integer>
-- MLIR:       ^[[E]]:
-- MLIR:         ada.return %arg2 : !ada.qual<i32, @standard.integer>

-- LLVM-LABEL: define i32 @_ada_if_stmt(
-- LLVM:         br i1 %0, label %{{.*}}, label %{{.*}}
-- LLVM:         ret i32 %1
-- LLVM:         ret i32 %2

function If_Stmt (Cond : Boolean; X, Y : Integer) return Integer is
begin
   if Cond then
      return X;
   else
      return Y;
   end if;
end If_Stmt;
