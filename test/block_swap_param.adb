-- RUN: %lalvm -O1 --emit=llvm --mlir-print-ir-after=canonicalize %s -o /dev/null 2>&1 | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm -O1 --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- Writable formal objects promote to values after reference lowering.
-- MLIR-LABEL: func.func @_ada_block_swap_param(
-- MLIR-NEXT:    return %arg1, %arg1, %arg0

-- LLVM-LABEL: define { i32, i32, i32 } @_ada_block_swap_param(i32 %0, i32 %1)
-- LLVM:         insertvalue { i32, i32, i32 } poison, i32 %1, 0
-- LLVM:         insertvalue { i32, i32, i32 } %{{.*}}, i32 %1, 1
-- LLVM:         insertvalue { i32, i32, i32 } %{{.*}}, i32 %0, 2
-- LLVM:         ret { i32, i32, i32 }

function Block_Swap_Param (U, V : in out Integer) return Integer is
begin
   Swap:
      declare
         Temp : Integer;
      begin
         Temp := V; V := U; U := Temp;
      end Swap;
   return U;
end Block_Swap_Param;
