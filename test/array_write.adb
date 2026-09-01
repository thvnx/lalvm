-- Writing an array element: the indexed destination `V (3)` routes through
-- `ada.index` for the element's location, and the value is stored through it.

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- CHECK: %[[ELT:.*]] = ada.index %{{.*}}[%{{.*}}] : (memref<!ada.qual<!ada.array<i32[i32 x 10]>, @array_write.vec>>, !ada.qual<i32, @standard.integer>) -> memref<!ada.qual<i32, @standard.integer>, strided<[], offset: ?>>
-- CHECK: memref.store %{{.*}}, %[[ELT]][] : memref<!ada.qual<i32, @standard.integer>, strided<[], offset: ?>>

procedure Array_Write is
   type Vec is array (1 .. 10) of Integer;
   V : Vec;
begin
   V (3) := 42;
end Array_Write;
