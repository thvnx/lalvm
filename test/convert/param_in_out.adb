-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- Scalar `in out` is passed by reference: only the callee converts.

-- CHECK: ada.coerce %{{.*}} : <i32, @param_in_out.small> to <i32, @standard.integer>
-- CHECK: ada.range_check %{{.*}}, %{{.*}} : !ada.qual<i32, @standard.integer> to !ada.qual<i32, @param_in_out.small>
-- CHECK-NOT: ada.coerce
-- CHECK: ada.call @param_in_out.inc(

procedure Param_In_Out is
   subtype Small is Integer range 1 .. 10;
   procedure Inc (X : in out Small) is
   begin
      X := X + 1;
   end Inc;
   N : Small := 5;
begin
   Inc (N);
end Param_In_Out;
