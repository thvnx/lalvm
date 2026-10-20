-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=ast %s | %FileCheck %s

-- An expression prints its expected type only when it differs from its own.

-- CHECK:      IntLiteral "3" [20:13-{{[0-9:]+}}] type=standard.universal_int_type_@{{[^ ]+}} expected=emit_ast_expected_type.s@17:4 static value=3{{$}}
-- CHECK:      Identifier "X" [21:19-{{[0-9:]+}}] ref={{[^ ]+}} type=emit_ast_expected_type.s@17:4 expected=standard.integer@{{[^ ]+}}{{$}}
-- CHECK:      RealLiteral "1.5" [22:14-{{[0-9:]+}}] type=standard.universal_real_type_@{{[^ ]+}} expected=emit_ast_expected_type.fx@18:4 static{{$}}
-- CHECK:      IntLiteral "2" [23:13-{{[0-9:]+}}] {{.*}} expected=emit_ast_expected_type.t@19:4 static value=2{{$}}
-- CHECK:      HandledStmts
-- CHECK:      Identifier "Y" [25:4-{{[0-9:]+}}] ref={{[^ ]+}} type={{[^ ]+}}{{$}}
-- CHECK:      Identifier "Y" [25:9-{{[0-9:]+}}] ref={{[^ ]+}} type={{[^ ]+}}{{$}}

procedure Emit_Ast_Expected_Type is
   subtype S is Integer range 1 .. 10;
   type Fx is delta 0.01 range 0.0 .. 10.0;
   type T is new Integer;
   X : S := 3;
   Y : Integer := X;
   Z : Fx := 1.5;
   W : T := 2 + T (X);
begin
   Y := Y;
end Emit_Ast_Expected_Type;
