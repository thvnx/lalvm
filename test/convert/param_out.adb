-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- Scalar `out` is passed by reference: only the callee converts.

-- CHECK: ada.range_check %{{.*}}, %{{.*}} : !ada.qual<i32, @standard.integer> to !ada.qual<i32, @param_out.small>
-- CHECK-NOT: ada.coerce
-- CHECK: ada.call @param_out.set(

procedure Param_Out (V : Integer) is
   subtype Small is Integer range 1 .. 10;
   procedure Set (X : out Small; Y : Integer) is
   begin
      X := Y;
   end Set;
   N : Small;
begin
   Set (N, V);
end Param_Out;
