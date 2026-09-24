-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

-- CHECK: expr_function_library_unit.adb:{{[0-9]+}}:{{[0-9]+}}: error: expression function cannot be a library unit

function Expr_Function_Library_Unit return Integer is (42);
