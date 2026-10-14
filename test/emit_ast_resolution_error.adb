-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=ast %s | %FileCheck %s

-- Libadalang raises on 'Bit.
-- CHECK:      DefiningName {{.*}}[{{[0-9]+}}:4-{{[0-9:]+}}] <resolution failed>{{$}}
-- CHECK:      AttributeRef [{{[0-9]+}}:19-{{[0-9:]+}}] <resolution failed>{{$}}

-- The statements still resolve.
-- CHECK:      HandledStmts
-- CHECK-NOT:  <resolution failed>
-- CHECK:      EndName {{.*}} ref={{[^ <]+}}{{$}}

procedure Emit_Ast_Resolution_Error is
   X : Integer := 0;
   Y : Integer := X'Bit;
begin
   Y := X;
end Emit_Ast_Resolution_Error;
