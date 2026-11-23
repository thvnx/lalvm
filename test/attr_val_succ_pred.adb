-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s
-- RUN: %lalvm --emit=llvm --mlir-print-ir-after=ada-lower-conversions %s -o /dev/null 2>&1 | %FileCheck %s --check-prefix=LOWER

-- Attributes use the base type, including outside the prefix subtype.

-- CHECK-LABEL: ada.subp private @attr_val_succ_pred.color.pred(
-- CHECK: cf.switch
-- CHECK-NEXT: default: ^[[INVALID:bb[0-9]+]],
-- CHECK-NEXT: 246: ^[[RED:bb[0-9]+]]
-- CHECK-NEXT: 42: ^[[GREEN:bb[0-9]+]]
-- CHECK-NEXT: 99: ^[[BLUE:bb[0-9]+]]
-- CHECK: ^[[INVALID]]:
-- CHECK-NEXT: ada.constraint_error invalid_data
-- CHECK: ^[[RED]]:
-- CHECK-NEXT: ada.constraint_error overflow
-- CHECK: ^[[GREEN]]:
-- CHECK-NEXT: %[[NEXT:.*]] = ada.constant : {{.*}} = -10
-- CHECK-NEXT: ada.return %[[NEXT]]
-- CHECK: ^[[BLUE]]:
-- CHECK-NEXT: %[[NEXT:.*]] = ada.constant : {{.*}} = 42
-- CHECK-NEXT: ada.return %[[NEXT]]

-- CHECK-LABEL: ada.subp private @attr_val_succ_pred.color.succ(
-- CHECK: cf.switch
-- CHECK-NEXT: default: ^[[INVALID:bb[0-9]+]],
-- CHECK-NEXT: 246: ^[[RED:bb[0-9]+]]
-- CHECK-NEXT: 42: ^[[GREEN:bb[0-9]+]]
-- CHECK-NEXT: 99: ^[[BLUE:bb[0-9]+]]
-- CHECK: ^[[INVALID]]:
-- CHECK-NEXT: ada.constraint_error invalid_data
-- CHECK: ^[[RED]]:
-- CHECK-NEXT: %[[NEXT:.*]] = ada.constant : {{.*}} = 42
-- CHECK-NEXT: ada.return %[[NEXT]]
-- CHECK: ^[[GREEN]]:
-- CHECK-NEXT: %[[NEXT:.*]] = ada.constant : {{.*}} = 99
-- CHECK-NEXT: ada.return %[[NEXT]]
-- CHECK: ^[[BLUE]]:
-- CHECK-NEXT: ada.constraint_error overflow

-- CHECK-LABEL: ada.subp private @attr_val_succ_pred.from_pos(
-- CHECK-NEXT: %[[VAL:.*]] = ada.convert implicit %n : !ada.qual<i32, @standard.integer> to !ada.qual<i8, @attr_val_succ_pred.color>
-- CHECK-NEXT: ada.return %[[VAL]]

-- LOWER-LABEL: func.func private @attr_val_succ_pred__from_pos(
-- LOWER: ada.constant : !ada.qual<i32, @standard.integer> = 0
-- LOWER-NEXT: ada.constant : !ada.qual<i32, @standard.integer> = 2
-- LOWER-NEXT: ada.range
-- LOWER-NEXT: %[[POS:.*]] = ada.range_check %n,
-- LOWER-NEXT: ada.unwrap %[[POS]]
-- LOWER: arith.constant 42 : i8
-- LOWER-NEXT: arith.select

-- CHECK-LABEL: ada.subp private @attr_val_succ_pred.following(
-- CHECK-NEXT: %[[RESULT:.*]] = ada.call @attr_val_succ_pred.color.succ(%c)
-- CHECK-NEXT: ada.return %[[RESULT]]

-- CHECK-LABEL: ada.subp private @attr_val_succ_pred.previous(
-- CHECK-NEXT: %[[RESULT:.*]] = ada.call @attr_val_succ_pred.color.pred(%c)
-- CHECK-NEXT: ada.return %[[RESULT]]

-- CHECK-LABEL: ada.subp private @attr_val_succ_pred.integer_succ(
-- CHECK-NEXT: %[[X:.*]] = ada.convert implicit %x : !ada.qual<i32, @standard.positive> to !ada.qual<i32, @standard.integer>
-- CHECK-NEXT: %[[ONE:.*]] = ada.constant : !ada.qual<i32, @standard.integer> = 1
-- CHECK-NEXT: %[[RESULT:.*]] = ada.binop "+" %[[X]], %[[ONE]] checks<overflow> :
-- CHECK-NEXT: ada.return %[[RESULT]]

-- CHECK-LABEL: ada.subp private @attr_val_succ_pred.integer_val(
-- CHECK-NEXT: %[[VAL:.*]] = ada.convert implicit %x : !ada.qual<i64, @standard.long_integer> to !ada.qual<i32, @standard.integer>
-- CHECK-NEXT: ada.return %[[VAL]]

-- CHECK-LABEL: ada.subp private @attr_val_succ_pred.byte_val(
-- CHECK-NEXT: %[[VAL:.*]] = ada.convert implicit %x : !ada.qual<i32, @standard.integer> to !ada.qual<i8, @attr_val_succ_pred.byte>
-- CHECK-NEXT: ada.return %[[VAL]]

-- CHECK-LABEL: ada.subp private @attr_val_succ_pred.byte_pred(
-- CHECK-NEXT: %[[ONE:.*]] = ada.constant : !ada.qual<i8, @attr_val_succ_pred.byte> = 1
-- CHECK-NEXT: %[[RESULT:.*]] = ada.binop "-" %x, %[[ONE]] :
-- CHECK-NEXT: ada.return %[[RESULT]]

-- Character literal enumerations follow declaration order too.
-- CHECK-LABEL: ada.subp private @attr_val_succ_pred.letter_succ(
-- CHECK-NEXT: %[[RESULT:.*]] = ada.call @attr_val_succ_pred.letters.succ(%x)
-- CHECK-NEXT: ada.return %[[RESULT]]

-- CHECK-LABEL: ada.subp private @attr_val_succ_pred.plain_pred(
-- CHECK-NEXT: %[[END:.*]] = ada.constant : {{.*}} = 0
-- CHECK-NEXT: %[[CHECKED:.*]] = ada.overflow_check "=" %x, %[[END]]
-- CHECK-NEXT: %[[ONE:.*]] = ada.constant : !ada.qual<i3, @attr_val_succ_pred.plain> = 1
-- CHECK-NEXT: ada.binop "-" %[[CHECKED]], %[[ONE]]

procedure Attr_Val_Succ_Pred is
   type Color is (Red, Green, Blue);
   for Color use (Red => -10, Green => 42, Blue => 99);
   subtype Tail is Color range Green .. Blue;
   subtype Small is Integer range 1 .. 10;
   type Byte is mod 256;
   type Letters is ('A', 'Z');
   type Plain is (First, Middle, Last);

   function From_Pos (N : Integer) return Color
   is (Tail'Val (N));
   function Following (C : Color) return Color
   is (Tail'Succ (C));
   function Previous (C : Color) return Color
   is (Tail'Pred (C));
   function Integer_Succ (X : Positive) return Integer
   is (Small'Succ (X));
   function Integer_Val (X : Long_Integer) return Integer
   is (Small'Val (X));
   function Byte_Val (X : Integer) return Byte
   is (Byte'Val (X));
   function Byte_Pred (X : Byte) return Byte
   is (Byte'Pred (X));
   function Letter_Succ (X : Letters) return Letters
   is (Letters'Succ (X));
   function Plain_Pred (X : Plain) return Plain
   is (Plain'Pred (X));
begin
   null;
end Attr_Val_Succ_Pred;
