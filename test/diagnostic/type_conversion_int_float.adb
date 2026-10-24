-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

-- Conversions between integer and float types are not supported yet.

-- CHECK: error: unsupported type conversion

function Type_Conversion_Int_Float (F : Float) return Integer is
begin
   return Integer (F);
end Type_Conversion_Int_Float;
