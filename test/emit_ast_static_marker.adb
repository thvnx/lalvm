-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=ast %s | %FileCheck %s

-- Static expressions print `static`.

-- CHECK:      BinOp [{{[0-9]+}}:20-{{[0-9:]+}}] {{.*}} static value=5{{$}}
-- CHECK-NEXT: IntLiteral "2" [{{[0-9]+}}:20-{{[0-9:]+}}] {{.*}} static value=2{{$}}
-- CHECK:      BinOp [{{[0-9]+}}:20-{{[0-9:]+}}] {{.*}}static{{$}}
-- CHECK-NEXT: RealLiteral "1.5" [{{[0-9]+}}:20-{{[0-9:]+}}] {{.*}}static{{$}}
-- CHECK:      ConcatOp [{{[0-9]+}}:27-{{[0-9:]+}}] {{.*}}static{{$}}
-- CHECK-NEXT: StringLiteral "\"ab\"" [{{[0-9]+}}:27-{{[0-9:]+}}] {{.*}}static{{$}}
-- CHECK:      AttributeRef [{{[0-9]+}}:26-{{[0-9:]+}}] {{.*}}static{{$}}
-- CHECK:      Identifier "N" [{{[0-9]+}}:19-{{[0-9:]+}}] {{.*}} static value=5{{$}}
-- CHECK:      BinOp [{{[0-9]+}}:9-{{[0-9:]+}}] type={{[^ ]+$}}
-- CHECK-NEXT: Identifier "V" [{{[0-9]+}}:9-{{[0-9:]+}}] ref={{[^ ]+}} type={{[^ ]+$}}

procedure Emit_Ast_Static_Marker is
   N : constant := 2 + 3;
   R : constant := 1.5 * 2.0;
   S : constant String := "ab" & "cd";
   F : constant Float := Float'Last;
   V : Integer := N;
begin
   V := V + 1;
end Emit_Ast_Static_Marker;
