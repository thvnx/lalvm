-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %not %lalvm --bind --emit=ast %s 2>&1 | %FileCheck %s

-- CHECK: Can't dump a Libadalang AST when binding

procedure Bind_Emit_Ast is
begin
   null;
end Bind_Emit_Ast;
