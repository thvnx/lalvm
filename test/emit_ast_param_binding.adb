-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=ast %s | %FileCheck %s

-- Positional and named actuals of calls and instantiations print their formal
-- while array indexes and attribute arguments print none.

-- CHECK:      ParamAssoc [28:24-{{[0-9:]+}}] formal=N{{$}}
-- CHECK:      ParamAssoc [31:7-{{[0-9:]+}}] formal=X{{$}}
-- CHECK-NEXT: IntLiteral
-- CHECK-NEXT: ParamAssoc [31:10-{{[0-9:]+}}] formal=Y{{$}}
-- CHECK:      ParamAssoc [32:7-{{[0-9:]+}}] formal=Y{{$}}
-- CHECK:      ParamAssoc [32:15-{{[0-9:]+}}] formal=X{{$}}
-- CHECK:      ParamAssoc [33:7-{{[0-9:]+}}] formal=X{{$}}
-- CHECK:      ParamAssoc [33:10-{{[0-9:]+}}] formal=Y{{$}}
-- CHECK:      ParamAssoc [34:12-{{[0-9:]+}}]{{$}}
-- CHECK:      ParamAssoc [35:22-{{[0-9:]+}}]{{$}}

procedure Emit_Ast_Param_Binding is
   type Arr is array (1 .. 3) of Integer;
   A : Arr := (others => 0);
   procedure P (X : Integer; Y : in out Integer) is null;
   generic
      N : Integer;
   package G is
   end G;
   package I is new G (N => 2);
   V : Integer := 0;
begin
   P (1, V);
   P (Y => V, X => 2);
   P (V, Y => V);
   V := A (2);
   V := Integer'Max (V, 1);
end Emit_Ast_Param_Binding;
