-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- Known limitation: the assignment to I (an "in out" parameter) performs an
-- SSA rebind in the symbol table only; the new value is never written back to
-- the caller. The test therefore does not check for a store of the result.

-- MLIR-LABEL: ada.proc @proc(
-- MLIR-SAME:    %arg0: i32, %arg1: i32, %arg2: i32
-- MLIR:         %[[R:.*]] = ada.add %arg1, %arg2 : i32
-- MLIR-NEXT:    ada.return

-- LLVM-LABEL: define void @proc(
-- LLVM:         %{{.*}} = add i32 %1, %2
-- LLVM:         ret void

procedure Proc (I, J, K : in out Integer) is
begin
   I := J + K;
end Proc;
