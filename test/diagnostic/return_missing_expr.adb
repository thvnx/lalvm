-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

-- A function return needs an expression (RM 6.5). Libadalang does not check it.

procedure Return_Missing_Expr is
   X : Integer := 0;

   function F return Integer is
   begin
      if X > 0 then
         -- CHECK: return_missing_expr.adb:[[#@LINE+1]]:10: error: missing expression in return from function
         return;
      end if;
      return 1;
   end F;
begin
   null;
end Return_Missing_Expr;
