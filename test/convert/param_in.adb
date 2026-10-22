-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- Scalar `in` actual, converted at the call site.

-- CHECK: %[[C:.*]] = ada.coerce %arg0 : <i32, @standard.integer> to <i32, @param_in.small>
-- CHECK: ada.range_check %[[C]]
-- CHECK: ada.call @param_in.f(

function Param_In (I : Integer) return Integer is
   subtype Small is Integer range 1 .. 10;
   function F (X : Small) return Integer is
   begin
      return 0;
   end F;
begin
   return F (I);
end Param_In;
