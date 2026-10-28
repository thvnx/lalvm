// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// Round-trip for the `checks<...>` group of `ada.binop` and `ada.unop`: it
// parses and prints back, `|`-separated, and only when nonempty.

// RUN: %lalvm --emit=mlir %s | %FileCheck --strict-whitespace %s

module {
  // CHECK-LABEL: ada.subp @checks
  ada.subp @checks(%a: !ada.qual<i32, @integer>,
                   %b: !ada.qual<i32, @integer>)
      -> !ada.qual<i32, @integer> {
    // CHECK: ada.binop "+" %arg0, %arg1 checks<overflow> : (!ada.qual<i32, @integer>, !ada.qual<i32, @integer>) -> !ada.qual<i32, @integer>
    %0 = ada.binop "+" %a, %b checks<overflow> : (!ada.qual<i32, @integer>, !ada.qual<i32, @integer>) -> !ada.qual<i32, @integer>

    // CHECK: ada.binop "*" %arg0, %arg1 checks<overflow|division> : (!ada.qual<i32, @integer>, !ada.qual<i32, @integer>) -> !ada.qual<i32, @integer>
    %1 = ada.binop "*" %a, %b checks<overflow|division> : (!ada.qual<i32, @integer>, !ada.qual<i32, @integer>) -> !ada.qual<i32, @integer>

    // No group when empty.
    // CHECK: ada.binop "-" %arg0, %arg1 : (!ada.qual<i32, @integer>, !ada.qual<i32, @integer>) -> !ada.qual<i32, @integer>
    %2 = ada.binop "-" %a, %b : (!ada.qual<i32, @integer>, !ada.qual<i32, @integer>) -> !ada.qual<i32, @integer>

    // CHECK: ada.unop "abs" %arg0 checks<overflow> : (!ada.qual<i32, @integer>) -> !ada.qual<i32, @integer>
    %3 = ada.unop "abs" %a checks<overflow> : (!ada.qual<i32, @integer>) -> !ada.qual<i32, @integer>

    // CHECK: ada.unop "-" %arg0 : (!ada.qual<i32, @integer>) -> !ada.qual<i32, @integer>
    %4 = ada.unop "-" %a : (!ada.qual<i32, @integer>) -> !ada.qual<i32, @integer>

    ada.return %0 : !ada.qual<i32, @integer>
  }
}
