-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- A dynamic subtype's range is elaborated once at its declaration (RM 3.2.2):
-- the bounds become an `ada.range` in the entry block (here even though the
-- subtype is unreferenced). The non-static bound is still recorded as `?`.
-- MLIR-LABEL: ada.subp @subtype_dynamic
-- MLIR:         %[[LO:.*]] = ada.constant : !ada.qual<i32, @standard.integer> = 1
-- MLIR:         ada.range %[[LO]], %{{.*}} : !ada.qual<i32, @standard.integer> -> !ada.range<i32, @subtype_dynamic.s>
-- MLIR:         ada.decls {
-- MLIR-NEXT:      ada.type @subtype_dynamic.s base @standard.integer : i32 = #ada.int_info<range 1 to ?>

-- LLVM-LABEL: define i32 @_ada_subtype_dynamic(

function Subtype_Dynamic (N : Integer) return Integer is
   subtype S is Integer range 1 .. N * 2;
begin
   return N;
end Subtype_Dynamic;
