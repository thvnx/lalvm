-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- An array index as statement is not a valid Ada (RM 6.4). Libadalang reports
-- no precise diagnostics for that error for now (while GNAT diagnoses:
-- "procedure or entry name expected").

-- RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

-- CHECK: error: {{.*}}name resolution failed with no diagnostic to report

procedure Array_Index_Stmt is
   type Vec is array (1 .. 10) of Integer;
   V : Vec;
begin
   V (3);
end Array_Index_Stmt;
