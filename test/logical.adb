-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- Boolean logical operators 'and', 'or', 'xor' (RM 4.5.1): operands and result
-- are Boolean, modelled by ada.binop and lowered to the bitwise i1 ops. They
-- carry no run-time checks. (Short-circuit 'and then'/'or else' and 'not' are
-- not yet supported.) Distinct operands keep the ops from folding.

-- MLIR-LABEL: ada.subp @logical
-- MLIR:         ada.binop "and" %arg0, %arg1 : !ada.qual<i1, @standard.boolean>
-- MLIR:         ada.binop "or" %arg0, %arg1 : !ada.qual<i1, @standard.boolean>
-- MLIR:         ada.binop "xor" %arg0, %arg1 : !ada.qual<i1, @standard.boolean>

-- LLVM-LABEL: define i1 @_ada_logical(
-- LLVM-DAG:      and i1 %0, %1
-- LLVM-DAG:      or i1 %0, %1
-- LLVM-DAG:      xor i1 %0, %1
-- LLVM:          ret i1

function Logical (X, Y : Boolean) return Boolean is
   A : Boolean := X and Y;
   B : Boolean := X or Y;
   C : Boolean := X xor Y;
begin
   return A;
end Logical;
