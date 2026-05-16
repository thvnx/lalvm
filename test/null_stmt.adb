-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.subp @null_stmt
-- MLIR:         ada.null
-- MLIR-NEXT:    ada.return

-- LLVM-LABEL: define void @_ada_null_stmt(
-- LLVM:          ret void

procedure Null_Stmt is
begin
   null;
end Null_Stmt;
