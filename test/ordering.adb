-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- Ordering operators '<', '<=', '>', '>=' (RM 4.5.2): operands share a type and
-- the result is Boolean. Modelled by ada.cmp (not ada.binop, whose result
-- matches its operands) and lowered to arith.cmpi -> icmp. A signed integer
-- type orders as signed (slt/sle/sgt/sge). Distinct operands keep the
-- comparisons from folding.

-- MLIR-LABEL: ada.subp @ordering
-- MLIR:         ada.cmp "<" %arg0, %arg1 : (!ada.qual<i32, @standard.integer>, !ada.qual<i32, @standard.integer>) -> !ada.qual<i1, @standard.boolean>
-- MLIR:         ada.cmp "<=" %arg0, %arg1 : (!ada.qual<i32, @standard.integer>, !ada.qual<i32, @standard.integer>) -> !ada.qual<i1, @standard.boolean>
-- MLIR:         ada.cmp ">" %arg0, %arg1 : (!ada.qual<i32, @standard.integer>, !ada.qual<i32, @standard.integer>) -> !ada.qual<i1, @standard.boolean>
-- MLIR:         ada.cmp ">=" %arg0, %arg1 : (!ada.qual<i32, @standard.integer>, !ada.qual<i32, @standard.integer>) -> !ada.qual<i1, @standard.boolean>

-- LLVM-LABEL: define i1 @_ada_ordering(
-- LLVM-DAG:      icmp slt i32 %0, %1
-- LLVM-DAG:      icmp sle i32 %0, %1
-- LLVM-DAG:      icmp sgt i32 %0, %1
-- LLVM-DAG:      icmp sge i32 %0, %1
-- LLVM:          ret i1

function Ordering (X, Y : Integer) return Boolean is
   A : Boolean := X < Y;
   B : Boolean := X <= Y;
   C : Boolean := X > Y;
   D : Boolean := X >= Y;
begin
   return A;
end Ordering;
