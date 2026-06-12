-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- A non-static bound is recorded as `?`; the static lower bound stays exact.
-- MLIR-LABEL: ada.subp @subtype_dynamic
-- MLIR:         ada.decls {
-- MLIR-NEXT:      ada.type @subtype_dynamic.s base @standard.integer : i32 = #ada.int_info<range 1 to ?>

-- LLVM-LABEL: define i32 @_ada_subtype_dynamic(

function Subtype_Dynamic (N : Integer) return Integer is
   subtype S is Integer range 1 .. N * 2;
begin
   return N;
end Subtype_Dynamic;
