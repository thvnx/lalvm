// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// RUN: %not %lalvm --emit=ast %s 2>&1 | %FileCheck %s

// CHECK: can't dump a Libadalang AST when the input is MLIR

module {
}
