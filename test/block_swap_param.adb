-- RUN: %lalvm -O1 --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm -O1 --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- The Swap block dissolves, but U and V are `in out` parameters backed by the
-- caller's storage, so they stay as `memref`/`ptr` and the swap propagates
-- through pointer loads and stores (only the local Temp is promoted).
-- MLIR-LABEL: ada.subp @block_swap_param(
-- MLIR:         %[[V0:.*]] = memref.load %arg1[] : memref<!ada.qual<i32, @standard.integer>>
-- MLIR:         %[[U0:.*]] = memref.load %arg0[] : memref<!ada.qual<i32, @standard.integer>>
-- MLIR:         memref.store %[[U0]], %arg1[] : memref<!ada.qual<i32, @standard.integer>>
-- MLIR:         memref.store %[[V0]], %arg0[] : memref<!ada.qual<i32, @standard.integer>>
-- MLIR:         %[[U1:.*]] = memref.load %arg0[] : memref<!ada.qual<i32, @standard.integer>>
-- MLIR:         ada.return %[[U1]] : !ada.qual<i32, @standard.integer>

-- LLVM-LABEL: define i32 @_ada_block_swap_param(ptr %0, ptr %1)
-- LLVM:         %[[V0:.*]] = load i32, ptr %1
-- LLVM:         %[[U0:.*]] = load i32, ptr %0
-- LLVM:         store i32 %[[U0]], ptr %1
-- LLVM:         store i32 %[[V0]], ptr %0
-- LLVM:         %[[U1:.*]] = load i32, ptr %0
-- LLVM:         ret i32 %[[U1]]

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
