-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- A parenthesized expression (RM 4.4) has the value of its operand; the
-- parentheses only group, so no op is emitted for them. The grouping does
-- affect evaluation order: (A + B) * C adds before multiplying.

-- MLIR-LABEL: ada.subp @paren_expr
-- MLIR:         %[[SUM:.*]] = ada.binop "+" %arg0, %arg1 checks<overflow> : !ada.qual<i32, @standard.integer>
-- MLIR-NEXT:    %[[R:.*]] = ada.binop "*" %[[SUM]], %arg2 checks<overflow> : !ada.qual<i32, @standard.integer>
-- MLIR-NEXT:    ada.return %[[R]] : !ada.qual<i32, @standard.integer>

-- LLVM-LABEL: define i32 @_ada_paren_expr(
-- LLVM:         %[[SUM:.*]] = add i32 %0, %1
-- LLVM-NEXT:    %{{.*}} = mul i32 %[[SUM]], %2
-- LLVM:         ret i32

function Paren_Expr (A, B, C : Integer) return Integer is
begin
   return (A + B) * C;
end Paren_Expr;
