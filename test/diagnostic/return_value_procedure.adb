-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

-- A procedure return has no expression (RM 6.5). Libadalang does not check it.

procedure Return_Value_Procedure is
begin
   -- CHECK: return_value_procedure.adb:[[#@LINE+1]]:4: error: procedure cannot return value (use function)
   return 1;
end Return_Value_Procedure;
