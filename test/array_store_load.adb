-- Store to an array element then load it back.

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- CHECK: %[[W:.*]] = ada.index %[[ARR:.*]][%{{.*}}] {{.*}}-> memref<!ada.qual<i32, @standard.integer>, strided<[], offset: ?>>
-- CHECK: memref.store %{{.*}}, %[[W]][] : memref<!ada.qual<i32, @standard.integer>, strided<[], offset: ?>>
-- CHECK: %[[R:.*]] = ada.index %[[ARR]][%{{.*}}] {{.*}}-> memref<!ada.qual<i32, @standard.integer>, strided<[], offset: ?>>
-- CHECK: %[[V:.*]] = memref.load %[[R]][] : memref<!ada.qual<i32, @standard.integer>, strided<[], offset: ?>>
-- CHECK: memref.store %[[V]], %{{.*}}[] : memref<!ada.qual<i32, @standard.integer>>

procedure Array_Store_Load is
   type Vec is array (1 .. 10) of Integer;
   V : Vec;
   X : Integer;
begin
   V (3) := 42;
   X := V (3);
end Array_Store_Load;
