-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- Return, to a subtype then back to the base type.

-- CHECK: %[[C:.*]] = ada.coerce %{{.*}} : <i32, @standard.integer> to <i32, @return_value.small>
-- CHECK: ada.range_check %[[C]]
-- CHECK: ada.coerce %{{.*}} : <i32, @return_value.small> to <i32, @standard.integer>

function Return_Value (X : Integer) return Integer is
   subtype Small is Integer range 1 .. 10;
   function Clamp (V : Integer) return Small is
   begin
      return V;
   end Clamp;
begin
   return Clamp (X);
end Return_Value;
