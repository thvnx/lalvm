-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=ast %s | %FileCheck %s

-- AST dump contains nodes reference and type information.

-- CHECK: "X" [12:4-12:5]{{( type=[^ ]*)?$}}
-- CHECK: "X" [14:11-14:12] ref=emit_ast_names.x@12:4 type=standard.integer@{{.+}}:{{[0-9]+}}:{{[0-9]+}}

function Emit_Ast_Names return Integer is
   X : Integer := 0;
begin
   return X;
end Emit_Ast_Names;
