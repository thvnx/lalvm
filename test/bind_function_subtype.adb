-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --bind --emit=llvm %s | %FileCheck %s

-- A function returning a subtype of Integer can be binded.

-- CHECK: define i32 @main(
-- CHECK:   call i32 @_ada_bind_function_subtype()

function Bind_Function_Subtype return Natural is
begin
   return 0;
end Bind_Function_Subtype;
