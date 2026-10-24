-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s
-- RUN: %lalvm --emit=llvm --mlir-print-ir-after=ada-lower-conversions %s -o /dev/null 2>&1 | %FileCheck %s --check-prefix=LOWERED

-- Named numbers, narrowed from universal types.

-- CHECK: ada.convert implicit %{{.*}} : !ada.qual<i512, @standard.universal_int_type_> to !ada.qual<i32, @standard.integer>
-- CHECK: ada.convert implicit %{{.*}} : !ada.qual<f128, @standard.universal_real_type_> to !ada.qual<f32, @standard.float>

-- LOWERED: arith.trunci %{{.*}} : i512 to i32
-- LOWERED: arith.truncf %{{.*}} : f128 to f32

procedure Narrowing is
   Max : constant := 200;
   Half : constant := 0.5;
   I : Integer := Max;
   F : Float := Half;
begin
   null;
end Narrowing;
