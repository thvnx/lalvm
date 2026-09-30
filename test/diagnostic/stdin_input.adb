-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir - < %s 2>&1 | %FileCheck %s
-- RUN: %not %lalvm --emit=mlir -P unused.gpr - < %s 2>&1 | %FileCheck %s --check-prefix=PROJECT

-- CHECK-NOT: file name does not match unit name
-- CHECK:     module @__stdin

-- PROJECT: can't read standard input when -P is used

procedure Stdin_Input is
begin
   null;
end Stdin_Input;
