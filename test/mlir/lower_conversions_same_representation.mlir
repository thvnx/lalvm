// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// LowerAdaConversions on conversions that keep the machine representation.

// RUN: %lalvm --emit=llvm --mlir-print-ir-after=ada-lower-conversions %s -o /dev/null 2>&1 | %FileCheck %s --implicit-check-not=ada.convert

module {
  ada.type @integer : i32 = #ada.int_info<range -2147483648 to 2147483647>
  ada.type @natural base @integer : i32 = #ada.int_info<range 0 to 2147483647>
  ada.type @positive base @integer : i32 = #ada.int_info<range 1 to 2147483647>
  ada.type @int base @integer : i32

  // CHECK-LABEL: IR Dump After {{.*}}ada-lower-conversions

  // The check establishes the target subtype.
  // CHECK-LABEL: ada.subp @to_constrained
  // CHECK:         %[[LO:.*]] = ada.constant : !ada.qual<i32, @integer> = 1
  // CHECK:         %[[HI:.*]] = ada.constant : !ada.qual<i32, @integer> = 2147483647
  // CHECK:         %[[R:.*]] = ada.range %[[LO]], %[[HI]] : !ada.qual<i32, @integer> -> !ada.range<i32, @positive>
  // CHECK:         %[[C:.*]] = ada.range_check %arg0, %[[R]] : !ada.qual<i32, @integer> to !ada.qual<i32, @positive>
  // CHECK:         ada.return %[[C]]
  ada.subp @to_constrained(%v: !ada.qual<i32, @integer>) -> !ada.qual<i32, @positive> {
    %r = ada.convert explicit %v : !ada.qual<i32, @integer> to !ada.qual<i32, @positive>
    ada.return %r : !ada.qual<i32, @positive>
  }

  // A source range not within the target's is checked too.
  // CHECK-LABEL: ada.subp @to_narrower
  // CHECK:         ada.range_check %arg0, %{{.*}} : !ada.qual<i32, @natural> to !ada.qual<i32, @positive>
  ada.subp @to_narrower(%v: !ada.qual<i32, @natural>) -> !ada.qual<i32, @positive> {
    %r = ada.convert implicit %v : !ada.qual<i32, @natural> to !ada.qual<i32, @positive>
    ada.return %r : !ada.qual<i32, @positive>
  }

  // No check into a base type or a renaming.
  // CHECK-LABEL: ada.subp @to_base
  // CHECK:         ada.requalify %arg0 : !ada.qual<i32, @positive> to !ada.qual<i32, @integer>
  ada.subp @to_base(%v: !ada.qual<i32, @positive>) -> !ada.qual<i32, @integer> {
    %r = ada.convert implicit %v : !ada.qual<i32, @positive> to !ada.qual<i32, @integer>
    ada.return %r : !ada.qual<i32, @integer>
  }
  // CHECK-LABEL: ada.subp @to_renaming
  // CHECK:         ada.requalify %arg0 : !ada.qual<i32, @integer> to !ada.qual<i32, @int>
  ada.subp @to_renaming(%v: !ada.qual<i32, @integer>) -> !ada.qual<i32, @int> {
    %r = ada.convert implicit %v : !ada.qual<i32, @integer> to !ada.qual<i32, @int>
    ada.return %r : !ada.qual<i32, @int>
  }

  // No check either from a source range within the target's.
  // CHECK-LABEL: ada.subp @to_wider
  // CHECK-NOT:     ada.range_check
  // CHECK:         ada.requalify %arg0 : !ada.qual<i32, @positive> to !ada.qual<i32, @natural>
  ada.subp @to_wider(%v: !ada.qual<i32, @positive>) -> !ada.qual<i32, @natural> {
    %r = ada.convert implicit %v : !ada.qual<i32, @positive> to !ada.qual<i32, @natural>
    ada.return %r : !ada.qual<i32, @natural>
  }

  // Nor from a constant within the target's range.
  // CHECK-LABEL: ada.subp @constant_in_range
  // CHECK-NOT:     ada.range_check
  // CHECK:         ada.requalify %{{.*}} : !ada.qual<i32, @integer> to !ada.qual<i32, @positive>
  ada.subp @constant_in_range() -> !ada.qual<i32, @positive> {
    %c = ada.constant : !ada.qual<i32, @integer> = 5
    %r = ada.convert implicit %c : !ada.qual<i32, @integer> to !ada.qual<i32, @positive>
    ada.return %r : !ada.qual<i32, @positive>
  }

  // A conversion to the identical type is removed.
  // CHECK-LABEL: ada.subp @identity
  // CHECK-NEXT:    ada.return %arg0
  ada.subp @identity(%v: !ada.qual<i32, @positive>) -> !ada.qual<i32, @positive> {
    %r = ada.convert explicit %v : !ada.qual<i32, @positive> to !ada.qual<i32, @positive>
    ada.return %r : !ada.qual<i32, @positive>
  }

  // A dynamic bound: the check uses the descriptor the conversion carries.
  // CHECK-LABEL: ada.subp @to_dynamic
  // CHECK:         %[[R:.*]] = ada.range %{{.*}}, %arg0
  // CHECK:         ada.range_check %arg1, %[[R]] : !ada.qual<i32, @integer> to !ada.qual<i32, @to_dynamic.s>
  ada.subp @to_dynamic(%n: !ada.qual<i32, @integer>, %v: !ada.qual<i32, @integer>) -> !ada.qual<i32, @to_dynamic.s> {
    %lo = ada.constant : !ada.qual<i32, @integer> = 1
    %r = ada.range %lo, %n : !ada.qual<i32, @integer> -> !ada.range<i32, @to_dynamic.s>
    ada.decls {
      ada.type @to_dynamic.s base @integer : i32 = #ada.int_info<range 1 to ?>
    }
    %c = ada.convert implicit %v, %r : !ada.qual<i32, @integer> to !ada.qual<i32, @to_dynamic.s>
    ada.return %c : !ada.qual<i32, @to_dynamic.s>
  }

  // From a nested subprogram: closure conversion captures the descriptor as a
  // parameter, and the enclosing subprogram's type still resolves. The lifted
  // subprogram is moved to the end of the module.
  // CHECK-LABEL: ada.subp private @outer.inner
  // CHECK-SAME:    %[[R:arg[0-9]+]]: !ada.range<i32, @outer.s>
  // CHECK:         ada.range_check %arg0, %[[R]] : !ada.qual<i32, @integer> to !ada.qual<i32, @outer.s>
  ada.subp @outer(%n: !ada.qual<i32, @integer>, %v: !ada.qual<i32, @integer>) -> !ada.qual<i32, @outer.s> {
    %lo = ada.constant : !ada.qual<i32, @integer> = 1
    %r = ada.range %lo, %n : !ada.qual<i32, @integer> -> !ada.range<i32, @outer.s>
    ada.decls {
      ada.type @outer.s base @integer : i32 = #ada.int_info<range 1 to ?>
      ada.subp private @outer.inner(%x: !ada.qual<i32, @integer>) -> !ada.qual<i32, @outer.s> {
        %c = ada.convert implicit %x, %r : !ada.qual<i32, @integer> to !ada.qual<i32, @outer.s>
        ada.return %c : !ada.qual<i32, @outer.s>
      }
    }
    %y = ada.call @outer.inner(%v) : (!ada.qual<i32, @integer>) -> !ada.qual<i32, @outer.s>
    ada.return %y : !ada.qual<i32, @outer.s>
  }
}
