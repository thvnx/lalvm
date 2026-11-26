-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm -O1 --emit=llvm %s | %FileCheck %s
-- XFAIL: libadalang-26

-- Libadalang 26 evaluates modular arithmetic without applying the modulus.
-- The named number must denote 1, not 11.
-- CHECK: ret i32 1{{$}}

function Number_Decl_Modular return Integer is
   type M10 is mod 10;
   N : constant := M10'(9) + M10'(2);
begin
   return N;
end Number_Decl_Modular;
