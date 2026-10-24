-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s
-- RUN: %lalvm --emit=llvm --mlir-print-ir-after=ada-lower-conversions %s -o /dev/null 2>&1 | %FileCheck %s --check-prefix=LOWERED

-- Scalar `in out` is passed by reference: only the callee converts.

-- CHECK: ada.coerce %{{.*}} : <i32, @param_in_out.small> to <i32, @standard.integer>
-- CHECK: ada.convert implicit %{{.*}} : !ada.qual<i32, @standard.integer> to !ada.qual<i32, @param_in_out.small>
-- CHECK-NOT: ada.coerce
-- CHECK-NOT: ada.convert
-- CHECK: ada.call @param_in_out.inc(

-- LOWERED: ada.range_check %{{.*}}, %{{.*}} : !ada.qual<i32, @standard.integer> to !ada.qual<i32, @param_in_out.small>

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
