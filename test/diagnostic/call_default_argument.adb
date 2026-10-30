-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s
-- CHECK: error: omitted default arguments are not supported

function Call_Default_Argument return Integer is
   function Value (X : Integer := 1) return Integer is
   begin
      return X;
   end Value;
begin
   return Value;
end Call_Default_Argument;
