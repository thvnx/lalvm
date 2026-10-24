-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s
-- RUN: %lalvm --emit=llvm --mlir-print-ir-after=ada-lower-conversions %s -o /dev/null 2>&1 | %FileCheck %s --check-prefix=LOWERED

-- Explicit type conversion.

-- CHECK: ada.convert explicit %arg0 : !ada.qual<i16, @standard.short_integer> to !ada.qual<i32, @standard.integer>

-- LOWERED-NOT: ada.range_check
-- LOWERED:     arith.extsi %{{.*}} : i16 to i32

function Explicit_Conversion (X : Short_Integer) return Integer is
begin
   return Integer (X);
end Explicit_Conversion;
