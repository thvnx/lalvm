-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- Equality and relational operators on an enumeration type (RM 4.5.2). They
-- compare the literals' positional codes, ordered unsigned (an enum orders by
-- its non-negative position). The operands are parameters of the nested `Cmp`
-- so the comparisons are not folded.

-- MLIR-LABEL: ada.subp private @enum_cmp.cmp
-- MLIR-DAG: ada.cmp "<" %arg0, %arg1 : (!ada.qual<i4, @enum_cmp.day>
-- MLIR-DAG: ada.cmp ">=" %arg1, %arg0 : (!ada.qual<i4, @enum_cmp.day>
-- MLIR-DAG: ada.cmp "/=" %arg0, %arg1 : (!ada.qual<i4, @enum_cmp.day>

-- LLVM-LABEL: define internal i1 @enum_cmp__cmp(
-- LLVM-DAG: icmp ult i4
-- LLVM-DAG: icmp uge i4
-- LLVM-DAG: icmp ne i4

function Enum_Cmp return Boolean is
   type Day is (Mon, Tue, Wed, Thu, Fri);
   function Cmp (A, B : Day) return Boolean is
   begin
      return A < B and B >= A and A /= B;
   end Cmp;
begin
   return Cmp (Wed, Fri);
end Enum_Cmp;
