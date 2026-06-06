-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- Inequality operator '/=' (RM 4.5.2): operands share a type and the result is
-- Boolean. Modelled by ada.cmp and lowered to arith.cmpi -> icmp ne. Exercised
-- with two operands (X /= Y) and with an integer literal resolved to the
-- operand type (X /= 42). Distinct operands are used so the comparison is not
-- folded away (X /= X would fold to a constant during lowering).

-- MLIR-LABEL: ada.subp @inequality
-- MLIR:         ada.cmp "/=" %arg0, %arg1 : (!ada.qual<i32, @standard.integer>, !ada.qual<i32, @standard.integer>) -> !ada.qual<i1, @standard.boolean>
-- MLIR:         %[[C:.*]] = ada.constant : !ada.qual<i32, @standard.integer> = 42
-- MLIR:         ada.cmp "/=" %arg0, %[[C]] : (!ada.qual<i32, @standard.integer>, !ada.qual<i32, @standard.integer>) -> !ada.qual<i1, @standard.boolean>

-- LLVM-LABEL: define i1 @_ada_inequality(
-- LLVM-DAG:      icmp ne i32 %0, %1
-- LLVM-DAG:      icmp ne i32 %0, 42
-- LLVM:          ret i1

function Inequality (X, Y : Integer) return Boolean is
   B1 : Boolean := X /= Y;
   B2 : Boolean := X /= 42;
begin
   return B1;
end Inequality;
