-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --implicit-check-not=.pos
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- Positions use base type declaration order, regardless of representation
-- codes or the bounds of a subtype prefix.

-- CHECK-LABEL: ada.subp @attr_pos(
-- CHECK: ada.subp private @attr_pos.color.pos(%[[ARG:.*]]: !ada.qual<i8, @attr_pos.color>)
-- CHECK-NEXT: %[[RAW:.*]] = ada.unwrap %[[ARG]]
-- CHECK-NEXT: cf.switch %[[RAW]] : i8, [
-- CHECK-NEXT: default: ^[[INVALID:bb[0-9]+]],
-- CHECK-NEXT: 246: ^[[RED:bb[0-9]+]],
-- CHECK-NEXT: 42: ^[[GREEN:bb[0-9]+]],
-- CHECK-NEXT: 99: ^[[BLUE:bb[0-9]+]]
-- CHECK: ^[[INVALID]]:
-- CHECK-NEXT: ada.constraint_error invalid_data
-- CHECK: ^[[RED]]:
-- CHECK-NEXT: {{.*}} = ada.constant : !ada.qual<i512, @standard.universal_int_type_> = 0
-- CHECK: ^[[GREEN]]:
-- CHECK-NEXT: {{.*}} = ada.constant : !ada.qual<i512, @standard.universal_int_type_> = 1
-- CHECK: ^[[BLUE]]:
-- CHECK-NEXT: {{.*}} = ada.constant : !ada.qual<i512, @standard.universal_int_type_> = 2

-- LLVM-LABEL: define internal i512 @attr_pos__color__pos(
-- LLVM: call void @__lalvm_rcheck_CE_Invalid_Data(
-- LLVM-NEXT: unreachable

-- CHECK: %[[BOOL:.*]] = ada.convert implicit %b : !ada.qual<i1, @standard.boolean> to !ada.qual<i32, @standard.integer>
-- CHECK-NEXT: ada.binop "+" %x, %[[BOOL]]
-- CHECK: ada.convert implicit %c : !ada.qual<i8, @standard.character> to !ada.qual<i32, @standard.integer>
-- CHECK: %[[E:.*]] = ada.read %e
-- CHECK-NEXT: ada.call @attr_pos.color.pos(%[[E]])
-- CHECK: %[[AGAIN:.*]] = ada.read %e
-- CHECK-NEXT: ada.call @attr_pos.color.pos(%[[AGAIN]])

-- Executable expressions containing 'Pos must avoid Libadalang's incorrect fold.
-- CHECK: %[[STATIC:.*]] = ada.convert implicit %{{.*}} : !ada.qual<i512, @standard.universal_int_type_> to !ada.qual<i512, @standard.root_types_.root_integer>
-- CHECK-NEXT: %[[ADDEND:.*]] = ada.constant : !ada.qual<i512, @standard.root_types_.root_integer> = 1
-- CHECK-NEXT: %[[SUM:.*]] = ada.binop "+" %[[STATIC]], %[[ADDEND]]
-- CHECK-NEXT: ada.convert explicit %[[SUM]] : !ada.qual<i512, @standard.root_types_.root_integer> to !ada.qual<i32, @standard.integer>

function Attr_Pos (X : Integer; B : Boolean; C : Character) return Integer is
   type Color is (Red, Green, Blue);
   for Color use (Red => -10, Green => 42, Blue => 99);
   subtype Tail is Color range Green .. Blue;
   E : Color := Green;
begin
   return
     Integer'Pos (X)
     + Boolean'Pos (B)
     + Character'Pos (C)
     + Tail'Pos (E)
     + Color'Pos (E)
     + Integer (Color'Pos (Green) + 1);
end Attr_Pos;
