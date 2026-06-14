-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- `in out` parameters are passed by reference (memref<T>): the callee
-- operates directly on the caller's alloca. Write-back is automatic.

-- MLIR-LABEL: ada.subp @proc(
-- MLIR-SAME:    %arg0: memref<!ada.qual<i32, @standard.integer>>, %arg1: memref<!ada.qual<i32, @standard.integer>>, %arg2: memref<!ada.qual<i32, @standard.integer>>
-- MLIR:         %[[J:.*]] = memref.load %arg1[] : memref<!ada.qual<i32, @standard.integer>>
-- MLIR:         %[[K:.*]] = memref.load %arg2[] : memref<!ada.qual<i32, @standard.integer>>
-- MLIR:         %[[R:.*]] = ada.binop "+" %[[J]], %[[K]] checks<overflow> : !ada.qual<i32, @standard.integer>
-- MLIR:         memref.store %[[R]], %arg0[] : memref<!ada.qual<i32, @standard.integer>>
-- MLIR:         ada.return

-- LLVM-LABEL: define void @_ada_proc(ptr
-- LLVM:         load i32, ptr
-- LLVM:         load i32, ptr
-- LLVM:         %{{.*}} = call { i32, i1 } @llvm.sadd.with.overflow.i32
-- LLVM:         store i32 %{{.*}}, ptr
-- LLVM:         ret void

procedure Proc (I, J, K : in out Integer) is
begin
   I := J + K;
end Proc;
