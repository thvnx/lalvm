-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s
-- RUN: %lalvm --emit=llvm --mlir-print-ir-after=ada-lower-conversions %s -o /dev/null 2>&1 | %FileCheck %s --check-prefix=LOWERED

-- Return, to a subtype then back to the base type.

-- CHECK: ada.convert implicit %{{.*}} : !ada.qual<i32, @standard.integer> to !ada.qual<i32, @return_value.small>
-- CHECK: ada.convert implicit %{{.*}} : !ada.qual<i32, @return_value.small> to !ada.qual<i32, @standard.integer>

-- LOWERED-LABEL: ada.subp @return_value(
-- LOWERED:         ada.requalify %{{.*}} : !ada.qual<i32, @return_value.small> to !ada.qual<i32, @standard.integer>
-- LOWERED-LABEL: ada.subp private @return_value.clamp(
-- LOWERED:         ada.range_check %arg0, %{{.*}} : !ada.qual<i32, @standard.integer> to !ada.qual<i32, @return_value.small>

function Return_Value (X : Integer) return Integer is
   subtype Small is Integer range 1 .. 10;
   function Clamp (V : Integer) return Small is
   begin
      return V;
   end Clamp;
begin
   return Clamp (X);
end Return_Value;
