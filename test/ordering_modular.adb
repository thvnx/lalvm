-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- A modular type orders as unsigned (RM 3.5.4): '<' lowers to an unsigned
-- compare (icmp ult), unlike a signed integer's slt. The comparison is on
-- parameters of the nested `Less`, so it is not folded.

-- MLIR: ada.cmp "<" %arg0, %arg1 : (!ada.qual<i8, @{{.*}}>, !ada.qual<i8, @{{.*}}>) -> !ada.qual<i1, @standard.boolean>

-- LLVM: icmp ult i8 %0, %1

function Ordering_Modular return Boolean is
   type Byte is mod 256;
   function Less (X, Y : Byte) return Boolean is
   begin
      return X < Y;
   end Less;
begin
   return Less (200, 100);
end Ordering_Modular;
