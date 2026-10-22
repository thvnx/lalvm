-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s
-- XFAIL: *

-- Explicit type conversion, not supported yet.

-- CHECK: ada.coerce %arg0 : <i16, @standard.short_integer> to <i32, @standard.integer>

function Explicit_Conversion (X : Short_Integer) return Integer is
begin
   return Integer (X);
end Explicit_Conversion;
