-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=llvm %s 2>&1 | %FileCheck %s --implicit-check-not=warning:

-- A static value of a modular type reads as unsigned: 200 lies in 100 .. 250,
-- so it neither warns nor is checked.

-- CHECK-LABEL: define {{.*}}@_ada_subtype_static_modular(
-- CHECK-NOT:     call void @__gnat_rcheck_CE_Range_Check(

procedure Subtype_Static_Modular is
   type Byte is mod 256;
   subtype High is Byte range 100 .. 250;
   H : High;
begin
   H := 200;
end Subtype_Static_Modular;
