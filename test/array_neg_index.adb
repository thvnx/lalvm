-- Indexing an array with a negative 'First: the zero-based offset is `index -
-- 'First`, so 'First (-5 here) is emitted as a signed constant and subtracted.
-- Exercises the signed-offset path; a wrong sign would emit a huge unsigned
-- 'First instead of -5.

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- CHECK: %[[FIRST:.*]] = ada.constant : !ada.qual<i32, @standard.integer> = -5
-- CHECK: %[[OFF:.*]] = ada.binop "-" %{{.*}}, %[[FIRST]]
-- CHECK: ada.index %{{.*}}[%[[OFF]]] : (memref<!ada.qual<!ada.array<i32[i32 x 11]>, @array_neg_index.vec>>, !ada.qual<i32, @standard.integer>) -> memref<!ada.qual<i32, @standard.integer>, strided<[], offset: ?>>

procedure Array_Neg_Index is
   type Vec is array (-5 .. 5) of Integer;
   V : Vec;
begin
   V (2) := 0;
end Array_Neg_Index;
