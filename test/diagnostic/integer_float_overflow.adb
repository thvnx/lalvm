-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

-- CHECK: error: integer constant out of range for 'f32'

function Integer_Float_Overflow return Float is
   Huge : constant := 2**128;
begin
   return Float (Huge);
end Integer_Float_Overflow;
