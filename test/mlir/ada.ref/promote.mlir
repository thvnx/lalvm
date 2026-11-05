// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// RUN: %lalvm --emit=mlir -O1 %s | %FileCheck %s --implicit-check-not=ada.object --implicit-check-not=ada.read --implicit-check-not=ada.write

module {
  ada.type @integer : i32 = #ada.int_info<range -2147483648 to 2147483647>
  ada.type @small base @integer : i32 = #ada.int_info<range 1 to 10>
  ada.type @nonzero : i32 = #ada.int_info<range 5 to 10>

  // The initializer is defined inside the entry block, not at its start.
  // CHECK-LABEL: ada.subp @initialized(
  // CHECK-SAME: %[[INPUT:.*]]: !ada.qual<i32, @integer>
  // CHECK: %[[INITIAL:.*]] = ada.requalify %[[INPUT]] : !ada.qual<i32, @integer> to !ada.qual<i32, @small>
  // CHECK-NEXT: ada.return %[[INITIAL]] : !ada.qual<i32, @small>
  ada.subp @initialized(%v: !ada.qual<i32, @integer>) -> !ada.qual<i32, @small> {
    %initial = ada.requalify %v : !ada.qual<i32, @integer> to !ada.qual<i32, @small>
    %x = ada.object : !ada.ref<!ada.qual<i32, @small>> <- %initial : !ada.qual<i32, @small>
    %value = ada.read %x : !ada.ref<!ada.qual<i32, @small>> -> !ada.qual<i32, @small>
    ada.return %value : !ada.qual<i32, @small>
  }

  // Writes in separate blocks supply the value at the join.
  // CHECK-LABEL: ada.subp @join(
  // CHECK-SAME: %[[COND:.*]]: i1, %[[LEFT:.*]]: !ada.qual<i32, @small>, %[[RIGHT:.*]]: !ada.qual<i32, @small>
  // CHECK: cf.cond_br %[[COND]], ^[[THEN:bb[0-9]+]], ^[[ELSE:bb[0-9]+]]
  // CHECK: ^[[THEN]]:
  // CHECK: %[[LBASE:.*]] = ada.requalify %[[LEFT]] : !ada.qual<i32, @small> to !ada.qual<i32, @integer>
  // CHECK: cf.br ^[[JOIN:bb[0-9]+]](%[[LBASE]] : !ada.qual<i32, @integer>)
  // CHECK: ^[[ELSE]]:
  // CHECK: %[[RBASE:.*]] = ada.requalify %[[RIGHT]] : !ada.qual<i32, @small> to !ada.qual<i32, @integer>
  // CHECK: cf.br ^[[JOIN]](%[[RBASE]] : !ada.qual<i32, @integer>)
  // CHECK: ^[[JOIN]](%[[VALUE:.*]]: !ada.qual<i32, @integer>):
  // CHECK-NEXT: ada.return %[[VALUE]] : !ada.qual<i32, @integer>
  ada.subp @join(%condition: i1, %left: !ada.qual<i32, @small>, %right: !ada.qual<i32, @small>) -> !ada.qual<i32, @integer> {
    %x = ada.object : !ada.ref<!ada.qual<i32, @small>>
    cf.cond_br %condition, ^left, ^right
  ^left:
    ada.write %left, %x : !ada.qual<i32, @small> -> !ada.ref<!ada.qual<i32, @small>>
    cf.br ^join
  ^right:
    ada.write %right, %x : !ada.qual<i32, @small> -> !ada.ref<!ada.qual<i32, @small>>
    cf.br ^join
  ^join:
    %value = ada.read %x : !ada.ref<!ada.qual<i32, @small>> -> !ada.qual<i32, @integer>
    ada.return %value : !ada.qual<i32, @integer>
  }

  // A missing initializer uses the same fallback as storage promotion.
  // CHECK-LABEL: ada.subp @default_value(
  // CHECK: %[[DEFAULT:.*]] = ada.constant : !ada.qual<i32, @nonzero> = 5
  // CHECK-NEXT: ada.return %[[DEFAULT]] : !ada.qual<i32, @nonzero>
  ada.subp @default_value() -> !ada.qual<i32, @nonzero> {
    %x = ada.object : !ada.ref<!ada.qual<i32, @nonzero>>
    %value = ada.read %x : !ada.ref<!ada.qual<i32, @nonzero>> -> !ada.qual<i32, @nonzero>
    ada.return %value : !ada.qual<i32, @nonzero>
  }
}
