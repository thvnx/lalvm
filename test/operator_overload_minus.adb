-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM
-- XFAIL: *

-- User defined unary "-" (RM 6.6): GNAT mangles it `Osubtract`, lalvm emits
-- `Ominus` (TODO).

-- MLIR: ada.subp private @"operator_overload_minus.-"(%{{.*}}: !ada.qual<i32, @standard.integer>) -> !ada.qual<i32, @standard.integer>

-- LLVM-LABEL: define internal i32 @operator_overload_minus__Osubtract(
-- LLVM:          ret i32

procedure Operator_Overload_Minus is
   function "-" (A : Integer) return Integer is
   begin
      return A;
   end "-";
begin
   null;
end Operator_Overload_Minus;
