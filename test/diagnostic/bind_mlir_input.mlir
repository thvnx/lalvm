// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// RUN: %not %lalvm --bind %s -o %t.o 2>&1 | %FileCheck %s

// CHECK: can't bind when the input is MLIR

module {
}
