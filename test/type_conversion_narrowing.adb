-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s
-- RUN: %lalvm --emit=llvm --mlir-print-ir-after=ada-lower-conversions %s -o /dev/null 2>&1 | %FileCheck %s --check-prefix=LOWERED

-- Explicit narrowing type conversion: checked before truncating.

-- CHECK: ada.convert explicit %x : !ada.qual<i32, @standard.integer> to !ada.qual<i16, @standard.short_integer>

-- LOWERED:      %[[C:.*]] = ada.range_check %x, %{{.*}} : !ada.qual<i32, @standard.integer> to !ada.qual<i32, @standard.integer>
-- LOWERED:      %[[U:.*]] = ada.unwrap %[[C]]
-- LOWERED-NEXT: arith.trunci %[[U]] : i32 to i16

function Type_Conversion_Narrowing (X : Integer) return Short_Integer is
begin
   return Short_Integer (X);
end Type_Conversion_Narrowing;
