-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- User defined binary "-" (RM 6.6): GNAT mangles it `Osubtract`.

-- MLIR: ada.subp private @"operator_overload_subtract.-"(%{{.*}}: !ada.qual<i32, @standard.integer>, %{{.*}}: !ada.qual<i32, @standard.integer>) -> !ada.qual<i32, @standard.integer>

-- LLVM-LABEL: define internal i32 @operator_overload_subtract__Osubtract(
-- LLVM:          ret i32

procedure Operator_Overload_Subtract is
   function "-" (A : Integer; B : Integer) return Integer is
   begin
      return A + B;
   end "-";
begin
   null;
end Operator_Overload_Subtract;
