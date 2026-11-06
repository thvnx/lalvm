-- Indexing retains the Ada index. Storage lowering subtracts the lower bound.

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- CHECK: %[[INDEX:.*]] = ada.constant : !ada.qual<i32, @standard.integer> = 2
-- CHECK-NOT: ada.binop "-"
-- CHECK: ada.index %{{.*}}[%[[INDEX]]] : (memref<!ada.qual<!ada.array<i32[i32 x 11]>, @array_neg_index.vec>>, !ada.qual<i32, @standard.integer>) -> !ada.ref<!ada.qual<i32, @standard.integer>>
-- LLVM: getelementptr [11 x i32], ptr %{{.*}}, i32 0, i64 7

procedure Array_Neg_Index is
   type Vec is array (-5 .. 5) of Integer;
   V : Vec;
begin
   V (2) := 0;
end Array_Neg_Index;
