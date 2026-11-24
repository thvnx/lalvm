-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm -O1 --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm -O1 --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- A subtype's ada.type links its immediate subtype mark and is emitted eagerly into the
-- local ada.decls, at its declaration point. type_info records only the
-- subtype's own constraint: unconstrained subtypes are pure renamings.
-- MLIR-LABEL: ada.subp @subtype_decl
-- MLIR:         ada.decls {
-- MLIR-NEXT:      ada.type @subtype_decl.small subtype @standard.integer : i32 = #ada.int_info<range 1 to 100>
-- MLIR-NEXT:      ada.type @subtype_decl.flag subtype @standard.boolean : i1
-- MLIR-NEXT:      ada.type @subtype_decl.int subtype @standard.integer : i32
-- MLIR-NEXT:      ada.type @subtype_decl.e6 : i4 = #ada.enum_info<"a" = 0, "b" = 1, "c" = 2, "d" = 3, "e" = 4, "f" = 5>
-- MLIR-NEXT:      ada.type @subtype_decl.mid subtype @subtype_decl.e6 : i4 = #ada.enum_info<range 1 to 3>
-- MLIR-NEXT:      ada.type @subtype_decl.b8 : i8 = #ada.int_info<mod 256>
-- MLIR-NEXT:      ada.type @subtype_decl.half subtype @subtype_decl.b8 : i8 = #ada.int_info<range 0 to 127>

-- MLIR-NEXT:      ada.type @subtype_decl.alias subtype @subtype_decl.small : i32
-- MLIR-NEXT:      ada.type @subtype_decl.alias2 subtype @subtype_decl.alias : i32
-- MLIR-NEXT:      ada.type @subtype_decl.distinct derived @standard.positive : i32 = #ada.int_info<range 1 to 2147483647>

-- LLVM-LABEL: define void @_ada_subtype_decl(

procedure Subtype_Decl is
   subtype Small is Integer range 1 .. 100;
   subtype Flag is Boolean;
   subtype Int is Integer;
   type E6 is (A, B, C, D, E, F);
   subtype Mid is E6 range B .. D;
   type B8 is mod 256;
   subtype Half is B8 range 0 .. 127;
   subtype Alias is Small;
   subtype Alias2 is Alias;
   type Distinct is new Positive;
   X : Alias2 := 42;
begin
   null;
end Subtype_Decl;
