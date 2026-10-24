// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// `ada.unop` may take an operand of a subtype of its result type.

// RUN: %lalvm --emit=mlir %s | %FileCheck %s

// CHECK: ada.unop "-" %arg0 checks<overflow> : (!ada.qual<i32, @small>) -> !ada.qual<i32, @integer>
// CHECK: ada.unop "abs" %{{.*}} checks<overflow> : (!ada.qual<i32, @integer>) -> !ada.qual<i32, @integer>

module {
  ada.type @integer : i32 = #ada.int_info<range -2147483648 to 2147483647>
  ada.type @small base @integer : i32 = #ada.int_info<range 1 to 10>
  ada.subp @neg(%a: !ada.qual<i32, @small>) -> !ada.qual<i32, @integer> {
    %0 = ada.unop "-" %a checks<overflow> : (!ada.qual<i32, @small>) -> !ada.qual<i32, @integer>
    %1 = ada.unop "abs" %0 checks<overflow> : (!ada.qual<i32, @integer>) -> !ada.qual<i32, @integer>
    ada.return %1 : !ada.qual<i32, @integer>
  }
}
