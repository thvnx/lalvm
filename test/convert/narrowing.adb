-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- Named numbers, narrowed from universal types.

-- CHECK: ada.coerce %{{.*}} : <i512, @standard.universal_int_type_> to <i32, @standard.integer>
-- CHECK: ada.coerce %{{.*}} : <f128, @standard.universal_real_type_> to <f32, @standard.float>

procedure Narrowing is
   Max : constant := 200;
   Half : constant := 0.5;
   I : Integer := Max;
   F : Float := Half;
begin
   null;
end Narrowing;
