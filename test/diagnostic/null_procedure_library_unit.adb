-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

-- CHECK: null_procedure_library_unit.adb:{{[0-9]+}}:{{[0-9]+}}: error: null procedure cannot be a library unit

procedure Null_Procedure_Library_Unit is null;
