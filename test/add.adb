-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.subp @add
-- MLIR:         %[[AB:.*]] = ada.binop "+" %arg0, %arg1 checks<overflow> : !ada.qual<i32, @standard.integer>
-- MLIR-NEXT:    %[[ABC:.*]] = ada.binop "+" %[[AB]], %arg2 checks<overflow> : !ada.qual<i32, @standard.integer>
-- MLIR-NEXT:    ada.return %[[ABC]] : !ada.qual<i32, @standard.integer>

-- LLVM-LABEL: define i32 @_ada_add(
-- LLVM:         call { i32, i1 } @llvm.sadd.with.overflow.i32(i32 %0, i32 %1)
-- LLVM:         call { i32, i1 } @llvm.sadd.with.overflow.i32(i32 %{{.*}}, i32 %2)
-- LLVM:         ret i32

function Add (I, J, K : Integer) return Integer is
begin
   return I + J + K;
end Add;
