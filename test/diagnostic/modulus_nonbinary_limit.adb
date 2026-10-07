-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %not %lalvm --emit=llvm %s 2>&1 | %FileCheck %s

-- A nonbinary modulus is at most System.Max_Nonbinary_Modulus
-- (RM 3.5.4).

-- CHECK: modulus_nonbinary_limit.adb:12:20: error: nonbinary modulus exceeds limit (2 ** 32 - 1)

procedure Modulus_Nonbinary_Limit is
   type Odd is mod 2 ** 32 + 1;
begin
   null;
end Modulus_Nonbinary_Limit;
