-- RUN: %lalvm -O1 --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm -O1 --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- A subtype's ada.type links its base type and is emitted eagerly into the
-- local ada.decls, at its declaration point. type_info records only the
-- subtype's own constraint: unconstrained subtypes are pure renamings.
-- MLIR-LABEL: ada.subp @subtype_decl
-- MLIR:         %[[C:.*]] = ada.constant : !ada.qual<i32, @subtype_decl.small> = 42
-- MLIR-NEXT:    ada.decls {
-- MLIR-NEXT:      ada.type @subtype_decl.small base @standard.integer : i32 = #ada.int_info<range 1 to 100>
-- MLIR-NEXT:      ada.type @subtype_decl.flag base @standard.boolean : i1
-- MLIR-NEXT:      ada.type @subtype_decl.int base @standard.integer : i32
-- MLIR-NEXT:      ada.type @subtype_decl.e6 : i4 = #ada.enum_info<"a" = 0, "b" = 1, "c" = 2, "d" = 3, "e" = 4, "f" = 5>
-- MLIR-NEXT:      ada.type @subtype_decl.mid base @subtype_decl.e6 : i4 = #ada.enum_info<range 1 to 3>
-- MLIR-NEXT:      ada.type @subtype_decl.b8 : i8 = #ada.int_info<mod 256>
-- MLIR-NEXT:      ada.type @subtype_decl.half base @subtype_decl.b8 : i8 = #ada.int_info<range 0 to 127>

-- LLVM-LABEL: define void @_ada_subtype_decl(

procedure Subtype_Decl is
   subtype Small is Integer range 1 .. 100;
   subtype Flag is Boolean;
   subtype Int is Integer;
   type E6 is (A, B, C, D, E, F);
   subtype Mid is E6 range B .. D;
   type B8 is mod 256;
   subtype Half is B8 range 0 .. 127;
   X : Small := 42;
begin
   null;
end Subtype_Decl;
