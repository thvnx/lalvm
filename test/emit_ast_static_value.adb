-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=ast %s | %FileCheck %s

-- Print the value of static expressions in the AST dump.

-- CHECK:      Identifier "Green" [{{[0-9]+}}:29-{{[0-9:]+}}] ref={{[^ ]+}} static value=1{{$}}
-- CHECK:      BinOp [{{[0-9]+}}:22-{{[0-9:]+}}] {{.*}} value=1267650600228229401496703205376{{$}}
-- CHECK:      Identifier "Blue" [{{[0-9]+}}:28-{{[0-9:]+}}] {{.*}} value=2{{$}}
-- CHECK:      BinOp [{{[0-9]+}}:30-{{[0-9:]+}}] {{.*}} value=9{{$}}
-- CHECK-NEXT: AttributeRef [{{[0-9]+}}:30-{{[0-9:]+}}] {{.*}} value=10{{$}}
-- CHECK:      AttributeRef [{{[0-9]+}}:39-{{[0-9:]+}}] {{.*}} value=1{{$}}
-- CHECK:      Identifier "N" [{{[0-9]+}}:21-{{[0-9:]+}}] {{.*}} value=9{{$}}
-- CHECK:      BinOp [{{[0-9]+}}:9-{{[0-9:]+}}] type=
-- CHECK-NOT:  value=
-- CHECK:      EndName

procedure Emit_Ast_Static_Value is
   type Color is (Red, Green, Blue);
   for Color use (Red => 1, Green => 5, Blue => 10);
   subtype S is Integer range 1 .. 10;
   Big : constant := 2**100;
   C   : constant Color := Blue;
   N   : constant Integer := S'Last - S'First;
   V   : Integer := N;
begin
   V := V + V;
end Emit_Ast_Static_Value;
