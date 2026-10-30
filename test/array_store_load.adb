-- Store to an array element then load it back.

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- CHECK: %[[W:.*]] = ada.index %[[ARR:.*]][%{{.*}}] {{.*}}-> !ada.ref<!ada.qual<i32, @standard.integer>>
-- CHECK: ada.write %{{.*}}, %[[W]] : !ada.qual<i32, @standard.integer> -> !ada.ref<!ada.qual<i32, @standard.integer>>
-- CHECK: %[[R:.*]] = ada.index %[[ARR]][%{{.*}}] {{.*}}-> !ada.ref<!ada.qual<i32, @standard.integer>>
-- CHECK: %[[V:.*]] = ada.read %[[R]] : !ada.ref<!ada.qual<i32, @standard.integer>> -> !ada.qual<i32, @standard.integer>
-- CHECK: ada.write %[[V]], %{{.*}} : !ada.qual<i32, @standard.integer> -> !ada.ref<!ada.qual<i32, @standard.integer>>

procedure Array_Store_Load is
   type Vec is array (1 .. 10) of Integer;
   V : Vec;
   X : Integer;
begin
   V (3) := 42;
   X := V (3);
end Array_Store_Load;
