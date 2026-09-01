-- Reading an array element routes through `ada.index` for the element's
-- location and a `memref.load` through it.

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- CHECK: %[[ELT:.*]] = ada.index %{{.*}}[%{{.*}}] : (memref<!ada.qual<!ada.array<i32[i32 x 10]>, @array_read.vec>>, !ada.qual<i32, @standard.integer>) -> memref<!ada.qual<i32, @standard.integer>, strided<[], offset: ?>>
-- CHECK: memref.load %[[ELT]][] : memref<!ada.qual<i32, @standard.integer>, strided<[], offset: ?>>

procedure Array_Read is
   type Vec is array (1 .. 10) of Integer;
   V : Vec;
   X : Integer;
begin
   X := V (3);
end Array_Read;
