-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- A modular type orders as unsigned (RM 3.5.4): each ordering operator lowers
-- to an unsigned compare (ult/ule/ugt/uge), unlike a signed integer's
-- slt/sle/sgt/sge. The comparisons are on parameters of nested functions, so
-- they are not folded.

-- MLIR: ada.cmp "<" %arg0, %arg1 : (!ada.qual<i8, @{{.*}}>, !ada.qual<i8, @{{.*}}>) -> !ada.qual<i1, @standard.boolean>
-- MLIR: ada.cmp "<=" %arg0, %arg1 : (!ada.qual<i8, @{{.*}}>, !ada.qual<i8, @{{.*}}>) -> !ada.qual<i1, @standard.boolean>
-- MLIR: ada.cmp ">" %arg0, %arg1 : (!ada.qual<i8, @{{.*}}>, !ada.qual<i8, @{{.*}}>) -> !ada.qual<i1, @standard.boolean>
-- MLIR: ada.cmp ">=" %arg0, %arg1 : (!ada.qual<i8, @{{.*}}>, !ada.qual<i8, @{{.*}}>) -> !ada.qual<i1, @standard.boolean>

-- LLVM-DAG: icmp ult i8 %0, %1
-- LLVM-DAG: icmp ule i8 %0, %1
-- LLVM-DAG: icmp ugt i8 %0, %1
-- LLVM-DAG: icmp uge i8 %0, %1

function Ordering_Modular return Boolean is
   type Byte is mod 256;

   function Less (X, Y : Byte) return Boolean is
   begin
      return X < Y;
   end Less;

   function Less_Eq (X, Y : Byte) return Boolean is
   begin
      return X <= Y;
   end Less_Eq;

   function Greater (X, Y : Byte) return Boolean is
   begin
      return X > Y;
   end Greater;

   function Greater_Eq (X, Y : Byte) return Boolean is
   begin
      return X >= Y;
   end Greater_Eq;
begin
   return Less (200, 100) or Less_Eq (200, 100)
     or Greater (200, 100) or Greater_Eq (200, 100);
end Ordering_Modular;
