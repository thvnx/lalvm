-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- A dynamic value flowing into a constrained subtype emits a Constraint_Check
-- (RM 11.5): the static bounds become an `ada.range`, checked by
-- `ada.range_check`. `Small` is a local subtype, so resolving its bounds
-- exercises the `ada.decls` scope walk. The base-type return widening is not
-- checked.

-- MLIR-LABEL: ada.subp @range_check_emit
-- MLIR:      ada.coerce %arg0 : <i32, @standard.integer> to <i32, @range_check_emit.small>
-- MLIR:      %[[LO:.*]] = arith.constant 1 : i32
-- MLIR:      %[[HI:.*]] = arith.constant 10 : i32
-- MLIR:      ada.range %[[LO]], %[[HI]] : !ada.range<i32, @range_check_emit.small>
-- MLIR:      ada.range_check %{{.*}}, %{{.*}} : !ada.qual<i32, @range_check_emit.small>

-- LLVM-LABEL: define i32 @_ada_range_check_emit(
-- LLVM:      icmp slt i32 %0, 1
-- LLVM:      icmp sgt i32 %0, 10
-- LLVM:      call void @__gnat_rcheck_CE_Range_Check(

function Range_Check_Emit (X : Integer) return Integer is
   subtype Small is Integer range 1 .. 10;
   Y : Small := X;
begin
   return Y;
end Range_Check_Emit;
