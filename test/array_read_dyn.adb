-- Reading an array element with a dynamic index: the index is loaded from a
-- variable, so the zero-based offset is computed at run time.

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- CHECK: %[[I:.*]] = memref.load %{{.*}}[] : memref<!ada.qual<i32, @standard.integer>>
-- CHECK: %[[SUB:.*]] = ada.coerce %[[I]] : {{.*}} to <i32, @array_read_dyn.__anonymous_vec_index_1>
-- CHECK: %[[CHK:.*]] = ada.index_check %[[SUB]], %{{.*}}
-- CHECK: %[[BASE:.*]] = ada.coerce %[[CHK]] : {{.*}} to <i32, @standard.integer>
-- CHECK: %[[OFF:.*]] = ada.binop "-" %[[BASE]], %{{.*}}
-- CHECK: ada.index %{{.*}}[%[[OFF]]] {{.*}}-> memref<!ada.qual<i32, @standard.integer>, strided<[], offset: ?>>

procedure Array_Read_Dyn is
   type Vec is array (1 .. 10) of Integer;
   V : Vec;
   I : Integer;
   X : Integer;
begin
   X := V (I);
end Array_Read_Dyn;
