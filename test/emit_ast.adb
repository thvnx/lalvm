-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=ast %s | %FileCheck %s

-- The file name is printed once, then one line per node with its source range
-- between brackets. Absent children and empty lists are not printed.

-- CHECK:      file emit_ast.adb
-- CHECK-NEXT: CompilationUnit [{{[0-9]+:[0-9]+-[0-9]+:[0-9]+}}]
-- CHECK:      "Emit_Ast" [{{[0-9]+:[0-9]+-[0-9]+:[0-9]+}}]
-- CHECK-NOT:  <null>
-- CHECK-NOT:  <empty list>

function Emit_Ast return Integer is
begin
   return 0;
end Emit_Ast;
