-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

-- Conversions of enumeration types are not supported yet.

-- CHECK: error: unsupported type conversion

function Type_Conversion_Enum (B : Boolean) return Boolean is
begin
   return Boolean (B);
end Type_Conversion_Enum;
